/*
 *  Copyright 2005,2006 Fabrice Colin
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <exception>
#include <iostream>
#include <fstream>
#include <sigc++/class_slot.h>
#include <sigc++/compatibility.h>
#include <sigc++/slot.h>
#include <glibmm/miscutils.h>

#include "MIMEScanner.h"
#include "StringManip.h"
#include "TimeConverter.h"
#include "Url.h"
#include "HtmlFilter.h"
#include "FilterFactory.h"
#include "FilterUtils.h"
#include "FilterWrapper.h"
#include "DBusXapianIndex.h"
#include "XapianDatabase.h"
#include "ActionQueue.h"
#include "CrawlHistory.h"
#include "QueryHistory.h"
#include "DownloaderFactory.h"
#include "SearchEngineFactory.h"
#include "config.h"
#include "NLS.h"
#include "PinotSettings.h"
#include "WorkerThreads.h"

using namespace SigC;
using namespace Glib;
using namespace std;

// A function object to stop threads with for_each()
struct StopThreadFunc
{
public:
	void operator()(map<WorkerThread *, Thread *>::value_type &p)
	{
		p.first->stop();
#ifdef DEBUG
		cout << "StopThreadFunc: stopped thread " << p.first->getId() << endl;
#endif
		Thread::yield();
	}
};

Dispatcher WorkerThread::m_dispatcher;
pthread_mutex_t WorkerThread::m_dispatcherMutex = PTHREAD_MUTEX_INITIALIZER;
bool WorkerThread::m_immediateFlush = true;

Dispatcher &WorkerThread::getDispatcher(void)
{
	return m_dispatcher;
}

void WorkerThread::immediateFlush(bool doFlush)
{
	m_immediateFlush = doFlush;
}

WorkerThread::WorkerThread() :
	m_id(0),
	m_background(false),
	m_done(false)
{
}

WorkerThread::~WorkerThread()
{
}

void WorkerThread::setId(unsigned int id)
{
	m_id = id;
}

unsigned int WorkerThread::getId(void)
{
	return m_id;
}

void WorkerThread::inBackground(void)
{
	m_background = true;
}

bool WorkerThread::isBackground(void)
{
	return m_background;
}

bool WorkerThread::operator<(const WorkerThread &other) const
{
	return m_id < other.m_id;
}

Glib::Thread *WorkerThread::start(void)
{
#ifdef DEBUG
	cout << "WorkerThread::start: " << getType() << " " << m_id << endl;
#endif
	// Create non-joinable threads
	return Thread::create(slot_class(*this, &WorkerThread::threadHandler), false);
}

bool WorkerThread::isDone(void) const
{
	return m_done;
}

string WorkerThread::getStatus(void) const
{
	return m_status;
}

void WorkerThread::threadHandler(void)
{
#ifdef DEBUG
	cout << "WorkerThread::threadHandler: thread " << m_id << " " << pthread_self() << endl;
#endif
	try
	{
		doWork();
	}
	catch (exception &ex)
	{
		cerr << "Exception in thread " << m_id << ", type " << getType()
			<< ":" << ex.what() << endl;
	}
	catch (...)
	{
		cerr << "Unknown exception in thread " << m_id << ", type " << getType() << endl;
	}

	emitSignal();
}

void WorkerThread::emitSignal(void)
{
	m_done = true;
	if (pthread_mutex_lock(&m_dispatcherMutex) == 0)
	{
#ifdef DEBUG
		cout << "WorkerThread::emitSignal: signaling end of thread " << m_id << endl;
#endif
		m_dispatcher();

		pthread_mutex_unlock(&m_dispatcherMutex);
	}
}

ThreadsManager::ThreadsManager(const string &defaultIndexLocation,
	unsigned int maxIndexThreads) :
	m_defaultIndexLocation(defaultIndexLocation),
	m_maxIndexThreads(maxIndexThreads),
	m_nextThreadId(1),
	m_backgroundThreadsCount(0),
	m_numCPUs(1)
{
	pthread_rwlock_init(&m_threadsLock, NULL);
	pthread_rwlock_init(&m_listsLock, NULL);

	m_numCPUs = sysconf(_SC_NPROCESSORS_ONLN);
}

ThreadsManager::~ThreadsManager()
{
	stop_threads();
	// Destroy the read/write locks
	pthread_rwlock_destroy(&m_listsLock);
	pthread_rwlock_destroy(&m_threadsLock);
}

bool ThreadsManager::read_lock_threads(void)
{
	if (pthread_rwlock_rdlock(&m_threadsLock) == 0)
	{
		return true;
	}

	return false;
}

bool ThreadsManager::write_lock_threads(void)
{
	if (pthread_rwlock_wrlock(&m_threadsLock) == 0)
	{
		return true;
	}

	return false;
}

void ThreadsManager::unlock_threads(void)
{
	pthread_rwlock_unlock(&m_threadsLock);
}

bool ThreadsManager::read_lock_lists(void)
{
	if (pthread_rwlock_rdlock(&m_listsLock) == 0)
	{
		return true;
	}

	return false;
}

bool ThreadsManager::write_lock_lists(void)
{
	if (pthread_rwlock_wrlock(&m_listsLock) == 0)
	{
		return true;
	}

	return false;
}

void ThreadsManager::unlock_lists(void)
{
	pthread_rwlock_unlock(&m_listsLock);
}

WorkerThread *ThreadsManager::get_thread(void)
{
	WorkerThread *pWorkerThread = NULL;

	// Get the first thread that's finished
	if (read_lock_threads() == true)
	{
		for (map<WorkerThread *, Thread *>::iterator threadIter = m_threads.begin();
			threadIter != m_threads.end(); ++threadIter)
		{
			if (threadIter->first->isDone() == true)
			{
				// This one will do...
				pWorkerThread = threadIter->first;
				// Remove it
				m_threads.erase(threadIter);
				break;
			}
#ifdef DEBUG
			cout << "ThreadsManager::get_thread: thread "
				<< threadIter->first->getId() << " is not done" << endl;
#endif
		}

		unlock_threads();
	}

	if (pWorkerThread == NULL)
	{
		return NULL;
	}

	if (pWorkerThread->isBackground() == true)
	{
#ifdef DEBUG
		cout << "ThreadsManager::get_thread: thread " << pWorkerThread->getId()
			<< " was running in the background" << endl;
#endif
		--m_backgroundThreadsCount;
	}

	return pWorkerThread;
}

ustring ThreadsManager::index_document(const DocumentInfo &docInfo)
{
	string location(docInfo.getLocation());

	if (location.empty() == true)
	{
		// Nothing to do
		return "";
	}

	// If the document is a mail message, we can't index it again
	Url urlObj(location);
	if (urlObj.getProtocol() == "mailbox")
	{
		return _("Can't index mail here");
	}

	// Is the document being indexed/updated ?
	if (write_lock_lists() == true)
	{
		bool beingProcessed = true;

		if (m_beingIndexed.find(location) == m_beingIndexed.end())
		{
			m_beingIndexed.insert(location);
			beingProcessed = false;
		}

		unlock_lists();

		if (beingProcessed == true)
		{
			// FIXME: we may have to set labels on this document
			ustring status(location);
			status += " ";
			status += _("is already being indexed");
			return status;
		}
	}

	// Is the document blacklisted ?
	if (PinotSettings::getInstance().isBlackListed(location) == true)
	{
		ustring status(location);
		status += " ";
		status += _("is blacklisted");
		return status;
	}

	start_thread(new IndexingThread(docInfo, m_defaultIndexLocation));

	return "";
}

void ThreadsManager::clear_queues(void)
{
	if (write_lock_lists() == true)
	{
		m_beingIndexed.clear();

		unlock_lists();

		ActionQueue actionQueue(PinotSettings::getInstance().getHistoryDatabaseName(), get_application_name());

		actionQueue.expireItems(time(NULL));

		m_backgroundThreadsCount = 0;
	}
}

bool ThreadsManager::start_thread(WorkerThread *pWorkerThread, bool inBackground)
{
	if (pWorkerThread == NULL)
	{
		return false;
	}

	pWorkerThread->setId(m_nextThreadId);
	if (inBackground == true)
	{
#ifdef DEBUG
		cout << "ThreadsManager::start_thread: thread " << pWorkerThread->getId()
			<< " will run in the background" << endl;
#endif
		pWorkerThread->inBackground();
		++m_backgroundThreadsCount;
	}

	// Start the thread
	Thread *pThread = pWorkerThread->start();
	if (pThread == NULL)
	{
		delete pWorkerThread;

		return false;
	}

	// Insert
	if (write_lock_threads() == true)
	{
		m_threads[pWorkerThread] = pThread;

		unlock_threads();
	}
	++m_nextThreadId;

	return true;
}

unsigned int ThreadsManager::get_threads_count(void)
{
	int count = 0;

	if (read_lock_threads() == true)
	{
		count = m_threads.size() - m_backgroundThreadsCount;

		unlock_threads();
	}
#ifdef DEBUG
	cout << "ThreadsManager::get_threads_count: " << count << " threads left" << endl;
#endif

	// A negative count would mean that a background thread
	// exited without signaling
	return (unsigned int)max(count , 0);
}

bool ThreadsManager::has_threads(void)
{
	if (m_threads.empty() == true)
	{
		return false;
	}

	return true;
}

void ThreadsManager::stop_threads(void)
{
	if (m_threads.empty() == false)
	{
		if (write_lock_threads() == true)
		{
			// Stop threads
			for_each(m_threads.begin(), m_threads.end(), StopThreadFunc());
			m_threads.clear();

			unlock_threads();
		}
	}
}

void ThreadsManager::connect(void)
{
	// The previous manager may have been signalled by our threads
	WorkerThread *pThread = get_thread();
	while (pThread != NULL)
	{
		m_onThreadEndSignal(pThread);

		// Next
		pThread = get_thread();
	}
#ifdef DEBUG
	cout << "ThreadsManager::connect: connecting" << endl;
#endif

	// Connect the dispatcher
	m_threadsEndConnection = WorkerThread::getDispatcher().connect(
		SigC::slot(*this, &ThreadsManager::on_thread_signal));
#ifdef DEBUG
	cout << "ThreadsManager::connect: connected" << endl;
#endif
}

void ThreadsManager::disconnect(void)
{
	m_threadsEndConnection.block();
	m_threadsEndConnection.disconnect();
#ifdef DEBUG
	cout << "ThreadsManager::disconnect: disconnected" << endl;
#endif
}

void ThreadsManager::on_thread_signal()
{
	WorkerThread *pThread = get_thread();
	if (pThread == NULL)
	{
#ifdef DEBUG
		cout << "ThreadsManager::on_thread_signal: foreign thread" << endl;
#endif
		return;
	}
	m_onThreadEndSignal(pThread);
}

ustring ThreadsManager::queue_index(const DocumentInfo &docInfo)
{
	double averageLoad[3];
	bool addToQueue = false;

#ifdef DEBUG
	cout << "ThreadsManager::queue_index: called" << endl;
#endif
	if (get_threads_count() >= m_maxIndexThreads)
	{
#ifdef DEBUG
		cout << "ThreadsManager::queue_index: too many threads" << endl;
#endif
		addToQueue = true;
	}
	// Get the load averaged over the last minute
	else if (getloadavg(averageLoad, 3) != -1)
	{
		// FIXME: is LOADAVG_1MIN Solaris specific ?
		if (averageLoad[0] >= (double)m_numCPUs * 4)
		{
			// Don't add to the load, queue this
			addToQueue = true;
		}
	}

	if (addToQueue == true)
	{
		ActionQueue actionQueue(PinotSettings::getInstance().getHistoryDatabaseName(), get_application_name());

		actionQueue.pushItem(ActionQueue::INDEX, docInfo);

		return "";
	}

	return index_document(docInfo);
}

bool ThreadsManager::pop_queue(const string &urlWasIndexed)
{
	bool getItem = true;
	bool foundItem = false;

#ifdef DEBUG
	cout << "ThreadsManager::pop_queue: called" << endl;
#endif
	if (get_threads_count() >= m_maxIndexThreads)
	{
#ifdef DEBUG
		cout << "ThreadsManager::pop_queue: too many threads" << endl;
#endif
		getItem = false;
	}

	if (write_lock_lists() == true)
	{
		// Update the in-progress list
		if (urlWasIndexed.empty() == false)
		{
			set<string>::iterator urlIter = m_beingIndexed.find(urlWasIndexed);
			if (urlIter != m_beingIndexed.end())
			{
				m_beingIndexed.erase(urlIter);
			}
		}

		unlock_lists();

		// Get an item ?
		if (getItem == true)
		{
			ActionQueue actionQueue(PinotSettings::getInstance().getHistoryDatabaseName(), get_application_name());
			ActionQueue::ActionType type;
			DocumentInfo docInfo;
			string previousLocation;

			while (actionQueue.popItem(type, docInfo) == true)
			{
				ustring status;

				if (type != ActionQueue::INDEX)
				{
					continue;
				}

				if (docInfo.getLocation() == previousLocation)
				{
					// Something dodgy is going on, we got the same item twice !
					status = previousLocation;
					status += " ";
					status += _("is already being indexed");
				}
				else
				{
					status = index_document(docInfo);
				}

				if (status.empty() == true)
				{
					foundItem = true;
					break;
				}

				previousLocation = docInfo.getLocation();
			}
		}
	}

	return foundItem;
}

void ThreadsManager::get_statistics(unsigned int &queueSize)
{
	if (read_lock_lists() == true)
	{
		// We want the number of documents being indexed,
		// not the number of document waiting in the queue
		queueSize = m_beingIndexed.size();

		unlock_lists();
	}
}

ListerThread::ListerThread(const string &indexName, unsigned int startDoc) :
	WorkerThread(),
	m_indexName(indexName),
	m_startDoc(startDoc),
	m_documentsCount(0)
{
}

ListerThread::~ListerThread()
{
}

string ListerThread::getType(void) const
{
	return "ListerThread";
}

string ListerThread::getIndexName(void) const
{
	return m_indexName;
}

unsigned int ListerThread::getStartDoc(void) const
{
	return m_startDoc;
}

const vector<DocumentInfo> &ListerThread::getDocuments(void) const
{
	return m_documentsList;
}

unsigned int ListerThread::getDocumentsCount(void) const
{
	return m_documentsCount;
}

IndexBrowserThread::IndexBrowserThread(const string &indexName,
	unsigned int maxDocsCount, unsigned int startDoc) :
	ListerThread(indexName, startDoc),
	m_maxDocsCount(maxDocsCount)
{
}

IndexBrowserThread::~IndexBrowserThread()
{
}

bool IndexBrowserThread::stop(void)
{
	m_done = true;
	return true;
}

void IndexBrowserThread::doWork(void)
{
	set<unsigned int> docIDList;
	set<string> docLabels;
	unsigned int numDocs = 0;

	const map<string, string> &indexesMap = PinotSettings::getInstance().getIndexes();
	map<string, string>::const_iterator mapIter = indexesMap.find(m_indexName);
	if (mapIter == indexesMap.end())
	{
		m_status = _("Index");
		m_status += " ";
		m_status += m_indexName;
		m_status += " ";
		m_status += _("doesn't exist");
		return;
	}

	// Get the index at that location
	IndexInterface *pIndex = PinotSettings::getInstance().getIndex(mapIter->second);
	if ((pIndex == NULL) ||
		(pIndex->isGood() == false))
	{
		m_status = _("Index error on");
		m_status += " ";
		m_status += mapIter->second;
		if (pIndex != NULL)
		{
			delete pIndex;
		}
		return;
	}

	m_documentsCount = pIndex->getDocumentsCount();
	if (m_documentsCount == 0)
	{
#ifdef DEBUG
		cout << "IndexBrowserThread::doWork: no documents" << endl;
#endif
		return;
	}

#ifdef DEBUG
	cout << "IndexBrowserThread::doWork: " << m_maxDocsCount << " off " << m_documentsCount
		<< " documents to browse, starting at position " << m_startDoc << endl;
#endif
	pIndex->listDocuments(docIDList, m_maxDocsCount, m_startDoc);

	m_documentsList.clear();
	m_documentsList.reserve(m_maxDocsCount);

	for (set<unsigned int>::iterator iter = docIDList.begin(); iter != docIDList.end(); ++iter)
	{
		if (m_done == true)
		{
			break;
		}

		// Get the document ID
		unsigned int docId = (*iter);

		DocumentInfo docInfo;
		if (pIndex->getDocumentInfo(docId, docInfo) == true)
		{
			string type = docInfo.getType();
			if (type.empty() == true)
			{
				type = "text/html";
			}

			DocumentInfo indexedDoc(docInfo.getTitle(), docInfo.getLocation(),
				type, docInfo.getLanguage());
			indexedDoc.setTimestamp(docInfo.getTimestamp());
			indexedDoc.setSize(docInfo.getSize());
			indexedDoc.setIsIndexed(PinotSettings::getInstance().getIndexId(m_indexName), docId);

			// Insert that document
			m_documentsList.push_back(indexedDoc);
			++numDocs;
		}
#ifdef DEBUG
		else cout << "IndexBrowserThread::doWork: couldn't retrieve document " << docId << endl;
#endif
	}
	delete pIndex;
}

QueryingThread::QueryingThread(const string &engineName, const string &engineDisplayableName,
	const string &engineOption, const QueryProperties &queryProps,
	unsigned int startDoc, bool listingIndex) :
	ListerThread(engineDisplayableName, startDoc),
	m_engineName(engineName),
	m_engineDisplayableName(engineDisplayableName),
	m_engineOption(engineOption),
	m_queryProps(queryProps),
	m_listingIndex(listingIndex),
	m_correctedSpelling(false)
{
#ifdef DEBUG
	cout << "QueryingThread::QueryingThread: engine " << m_engineName << ", " << engineOption
		<< ", mode " << m_listingIndex << endl;
#endif
}

QueryingThread::~QueryingThread()
{
}

string QueryingThread::getType(void) const
{
	if (m_listingIndex == true)
	{
		return ListerThread::getType();
	}

	return "QueryingThread";
}

string QueryingThread::getEngineName(void) const
{
	return m_engineDisplayableName;
}

QueryProperties QueryingThread::getQuery(bool &wasCorrected) const
{
	wasCorrected = m_correctedSpelling;
	return m_queryProps;
}

string QueryingThread::getCharset(void) const
{
	return m_resultsCharset;
}

bool QueryingThread::stop(void)
{
	m_done = true;
	return true;
}

void QueryingThread::processResults(const vector<DocumentInfo> &resultsList)
{
	PinotSettings &settings = PinotSettings::getInstance();
	IndexInterface *pDocsIndex = NULL;
	IndexInterface *pDaemonIndex = NULL;
	unsigned int indexId = 0;
	bool isIndexQuery = false;

	// Are we querying an index ?
	if (m_engineName == "xapian")
	{
		// Internal index ?
		if (m_engineOption == settings.m_docsIndexLocation)
		{
			indexId = settings.getIndexId(_("My Web Pages"));
			isIndexQuery = true;
		}
		else if (m_engineOption == settings.m_daemonIndexLocation)
		{
			indexId = settings.getIndexId(_("My Documents"));
			isIndexQuery = true;
		}
	}

	// Will we have to query internal indices ?
	if (isIndexQuery == false)
	{
		pDocsIndex = settings.getIndex(settings.m_docsIndexLocation);
		pDaemonIndex = settings.getIndex(settings.m_daemonIndexLocation);
	}

	// Copy the results list
	for (vector<DocumentInfo>::const_iterator resultIter = resultsList.begin();
		resultIter != resultsList.end(); ++resultIter)
	{
		DocumentInfo current(*resultIter);
		string title(_("No title"));
		string location(current.getLocation());
		string language(current.getLanguage());
		unsigned int docId = 0;

		// The title may contain formatting
		if (current.getTitle().empty() == false)
		{
			title = FilterUtils::stripMarkup(current.getTitle());
		}
		current.setTitle(title);
#ifdef DEBUG
		cout << "QueryingThread::processResults: title is " << title << endl;
#endif

		// Use the query's language if the result's is unknown
		if (language.empty() == true)
		{
			language = m_queryProps.getLanguage();
		}
		current.setLanguage(language);

		if (isIndexQuery == true)
		{
			unsigned int tmpId = 0;

			// The index engine should have set this
			docId = current.getIsIndexed(tmpId);
		}

		// Is this in one of the indexes ?
		if ((pDocsIndex != NULL) &&
			(pDocsIndex->isGood() == true))
		{
			docId = pDocsIndex->hasDocument(location);
			if (docId > 0)
			{
				indexId = settings.getIndexId(_("My Web Pages"));
			}
		}
		if ((pDaemonIndex != NULL) &&
			(pDaemonIndex->isGood() == true) &&
			(docId == 0))
		{
			docId = pDaemonIndex->hasDocument(location);
			if (docId > 0)
			{
				indexId = settings.getIndexId(_("My Documents"));
			}
		}

		if (docId > 0)
		{
			current.setIsIndexed(indexId, docId);
#ifdef DEBUG
			cout << "QueryingThread::processResults: found in index " << indexId << endl;
#endif
		}
#ifdef DEBUG
		else cout << "QueryingThread::processResults: not found in any index" << endl;
#endif

		m_documentsList.push_back(current);
	}

	if (pDocsIndex != NULL)
	{
		delete pDocsIndex;
	}
	if (pDaemonIndex != NULL)
	{
		delete pDaemonIndex;
	}
}

void QueryingThread::processResults(const vector<DocumentInfo> &resultsList,
	unsigned int indexId)
{
	unsigned int zeroId = 0;

	// Copy the results list
	for (vector<DocumentInfo>::const_iterator resultIter = resultsList.begin();
		resultIter != resultsList.end(); ++resultIter)
	{
		DocumentInfo current(*resultIter);

		// The engine has no notion of index IDs
		unsigned int docId = current.getIsIndexed(zeroId);
		current.setIsIndexed(indexId, docId);

		m_documentsList.push_back(current);
	}
}

void QueryingThread::doWork(void)
{
	PinotSettings &settings = PinotSettings::getInstance();

	// Get the SearchEngine
	SearchEngineInterface *pEngine = SearchEngineFactory::getSearchEngine(m_engineName, m_engineOption);
	if (pEngine == NULL)
	{
		m_status = _("Couldn't create search engine");
		m_status += " ";
		m_status += m_engineDisplayableName;
		return;
	}

	// Set up the proxy
	DownloaderInterface *pDownloader = pEngine->getDownloader();
	if ((pDownloader != NULL) &&
		(settings.m_proxyEnabled == true) &&
		(settings.m_proxyAddress.empty() == false))
	{
		char portStr[64];

		pDownloader->setSetting("proxyaddress", settings.m_proxyAddress);
		snprintf(portStr, 64, "%u", settings.m_proxyPort);
		pDownloader->setSetting("proxyport", portStr);
		pDownloader->setSetting("proxytype", settings.m_proxyType);
	}

	// Run the query
	pEngine->setDefaultOperator(SearchEngineInterface::DEFAULT_OP_AND);
	if (pEngine->runQuery(m_queryProps, m_startDoc) == false)
	{
		m_status = _("Couldn't run query on search engine");
		m_status += " ";
		m_status += m_engineDisplayableName;
	}
	else
	{
		const vector<DocumentInfo> &resultsList = pEngine->getResults();

		m_documentsList.clear();
		m_documentsList.reserve(resultsList.size());
		m_documentsCount = pEngine->getResultsCountEstimate();
#ifdef DEBUG
		cout << "QueryingThread::doWork: " << resultsList.size() << " off " << m_documentsCount
			<< " results to process, starting at position " << m_startDoc << endl;
#endif

		m_resultsCharset = pEngine->getResultsCharset();
		if (m_listingIndex == false)
		{
			processResults(resultsList);
		}
		else
		{
			processResults(resultsList,
				PinotSettings::getInstance().getIndexId(m_engineDisplayableName));
		}

		// Any spelling correction ?
		string correctedFreeQuery(pEngine->getSpellingCorrection());
		if (correctedFreeQuery.empty() == false)
		{
			m_correctedSpelling = true;
			m_queryProps.setFreeQuery(correctedFreeQuery);
		}
	}

	delete pEngine;
}

ExpandQueryThread::ExpandQueryThread(const QueryProperties &queryProps,
	const set<string> &relevantDocs) :
	WorkerThread(),
	m_queryProps(queryProps)
{
	copy(relevantDocs.begin(), relevantDocs.end(),
		inserter(m_relevantDocs, m_relevantDocs.begin()));
}

ExpandQueryThread::~ExpandQueryThread()
{
}

string ExpandQueryThread::getType(void) const
{
	return "ExpandQueryThread";
}

QueryProperties ExpandQueryThread::getQuery(void) const
{
	return m_queryProps;
}

const set<string> &ExpandQueryThread::getExpandTerms(void) const
{
	return m_expandTerms;
}

bool ExpandQueryThread::stop(void)
{
	m_done = true;
	return true;
}

void ExpandQueryThread::doWork(void)
{
	IndexInterface *pIndex = PinotSettings::getInstance().getIndex("MERGED");
	set<unsigned int> relevantDocIds;

	if ((pIndex == NULL) ||
		(pIndex->isGood() == false))
	{
		m_status = _("Index error on");
		m_status += " MERGED";
		if (pIndex != NULL)
		{
			delete pIndex;
		}
		return;
	}

	for (set<string>::iterator locationIter = m_relevantDocs.begin();
		locationIter != m_relevantDocs.end(); ++locationIter)
	{
		relevantDocIds.insert(pIndex->hasDocument(*locationIter));
	}

	delete pIndex;

	// Get the SearchEngine
	SearchEngineInterface *pEngine = SearchEngineFactory::getSearchEngine("xapian", "MERGED");
	if (pEngine == NULL)
	{
		m_status = _("Couldn't create search engine");
		m_status += " ";
		m_status += m_queryProps.getName();
		return;
	}

	// Set whether to expand the query
	pEngine->setQueryExpansion(relevantDocIds);

	// Run the query
	pEngine->setDefaultOperator(SearchEngineInterface::DEFAULT_OP_AND);
	if (pEngine->runQuery(m_queryProps) == false)
	{
		m_status = _("Couldn't run query on search engine");
	}
	else
	{
		// Copy the expand terms
		const set<string> &expandTerms = pEngine->getExpandTerms();
		copy(expandTerms.begin(), expandTerms.end(),
			inserter(m_expandTerms, m_expandTerms.begin()));
	}

	delete pEngine;
}

LabelUpdateThread::LabelUpdateThread(const set<string> &labelsToDelete,
	const map<string, string> &labelsToRename) :
	WorkerThread()
{
	copy(labelsToDelete.begin(), labelsToDelete.end(), inserter(m_labelsToDelete, m_labelsToDelete.begin()));
	copy(labelsToRename.begin(), labelsToRename.end(), inserter(m_labelsToRename, m_labelsToRename.begin()));
}

LabelUpdateThread::LabelUpdateThread(const string &labelName,
	const set<unsigned int> &docsIds, const set<unsigned int> &daemonIds) :
	WorkerThread(),
	m_labelName(labelName)
{
	copy(docsIds.begin(), docsIds.end(), inserter(m_docsIds, m_docsIds.begin()));
	copy(daemonIds.begin(), daemonIds.end(), inserter(m_daemonIds, m_daemonIds.begin()));
}

LabelUpdateThread::~LabelUpdateThread()
{
}

string LabelUpdateThread::getType(void) const
{
	return "LabelUpdateThread";
}

bool LabelUpdateThread::stop(void)
{
	m_done = true;
	return true;
}

void LabelUpdateThread::doWork(void)
{
	IndexInterface *pDocsIndex = PinotSettings::getInstance().getIndex(PinotSettings::getInstance().m_docsIndexLocation);
	if (pDocsIndex == NULL)
	{
		m_status = _("Index error on");
		m_status += " ";
		m_status += PinotSettings::getInstance().m_docsIndexLocation;
		return;
	}

	IndexInterface *pDaemonIndex = PinotSettings::getInstance().getIndex(PinotSettings::getInstance().m_daemonIndexLocation);
	if (pDaemonIndex == NULL)
	{
		m_status = _("Index error on");
		m_status += " ";
		m_status += PinotSettings::getInstance().m_daemonIndexLocation;
		delete pDocsIndex;
		return;
	}

	// Apply the new label to existing documents
	if (m_labelName.empty() == false)
	{
		set<string> labels;

		labels.insert(m_labelName);
		if (m_docsIds.empty() == false)
		{
			pDocsIndex->setDocumentsLabels(m_docsIds, labels, false);
		}
		if (m_daemonIds.empty() == false)
		{
			pDaemonIndex->setDocumentsLabels(m_daemonIds, labels, false);
		}
	}
	// Delete labels
	for (set<string>::iterator iter = m_labelsToDelete.begin(); iter != m_labelsToDelete.end(); ++iter)
	{
		pDocsIndex->deleteLabel(*iter);
		pDaemonIndex->deleteLabel(*iter);
	}
	// Rename labels
	for (map<string, string>::iterator iter = m_labelsToRename.begin(); iter != m_labelsToRename.end(); ++iter)
	{
		pDocsIndex->renameLabel(iter->first, iter->second);
		pDaemonIndex->renameLabel(iter->first, iter->second);
	}

	delete pDaemonIndex;
	delete pDocsIndex;
}

DownloadingThread::DownloadingThread(const DocumentInfo &docInfo) :
	WorkerThread(),
	m_docInfo(docInfo),
	m_pDoc(NULL),
	m_pDownloader(NULL)
{
}

DownloadingThread::~DownloadingThread()
{
	if (m_pDoc != NULL)
	{
		delete m_pDoc;
	}
	if (m_pDownloader != NULL)
	{
		delete m_pDownloader;
	}
}

string DownloadingThread::getType(void) const
{
	return "DownloadingThread";
}

string DownloadingThread::getURL(void) const
{
	return m_docInfo.getLocation();
}

const Document *DownloadingThread::getDocument(void) const
{
	return m_pDoc;
}

bool DownloadingThread::stop(void)
{
	m_done = true;
	return true;
}

void DownloadingThread::doWork(void)
{
	if (m_pDownloader != NULL)
	{
		delete m_pDownloader;
		m_pDownloader = NULL;
	}

	Url thisUrl(m_docInfo.getLocation());

	// Get a Downloader, the default one will do
	m_pDownloader = DownloaderFactory::getDownloader(thisUrl.getProtocol());
	if (m_pDownloader == NULL)
	{
		m_status = _("Couldn't obtain downloader for protocol");
		m_status += " ";
		m_status += thisUrl.getProtocol();
	}
	else if (m_done == false)
	{
		PinotSettings &settings = PinotSettings::getInstance();

		// Set up the proxy
		if ((settings.m_proxyEnabled == true) &&
			(settings.m_proxyAddress.empty() == false))
		{
			char portStr[64];

			m_pDownloader->setSetting("proxyaddress", settings.m_proxyAddress);
			snprintf(portStr, 64, "%u", settings.m_proxyPort);
			m_pDownloader->setSetting("proxyport", portStr);
			m_pDownloader->setSetting("proxytype", settings.m_proxyType);
		}

		m_pDoc = m_pDownloader->retrieveUrl(m_docInfo);
	}

	if (m_pDoc == NULL)
	{
		m_status = _("Couldn't retrieve");
		m_status += " ";
		m_status += m_docInfo.getLocation();
	}
}

IndexingThread::IndexingThread(const DocumentInfo &docInfo, const string &indexLocation,
	bool allowAllMIMETypes) :
	DownloadingThread(docInfo),
	m_docInfo(docInfo),
	m_docId(0),
	m_indexLocation(indexLocation),
	m_allowAllMIMETypes(allowAllMIMETypes),
	m_update(false)
{
}

IndexingThread::~IndexingThread()
{
}

string IndexingThread::getType(void) const
{
	return "IndexingThread";
}

const DocumentInfo &IndexingThread::getDocumentInfo(void) const
{
	return m_docInfo;
}

unsigned int IndexingThread::getDocumentID(void) const
{
	return m_docId;
}

bool IndexingThread::isNewDocument(void) const
{
	// If the thread is set to perform an update, the document isn't new
	if (m_update == true)
	{
		return false;
	}
	return true;
}

bool IndexingThread::stop(void)
{
	if (DownloadingThread::stop() == true)
	{
		m_done = true;
		return true;
	}

	return false;
}

void IndexingThread::doWork(void)
{
	IndexInterface *pIndex = PinotSettings::getInstance().getIndex(m_indexLocation);
	Url thisUrl(m_docInfo.getLocation());
	bool doDownload = true;

	// First things first, get the index
	if ((pIndex == NULL) ||
		(pIndex->isGood() == false))
	{
		m_status = _("Index error on");
		m_status += " ";
		m_status += m_indexLocation;
		if (pIndex != NULL)
		{
			delete pIndex;
		}
		return;
	}

	// Is it an update ?
	m_docId = pIndex->hasDocument(m_docInfo.getLocation());
	if (m_docId > 0)
	{
		// Ignore robots directives on updates
		m_update = true;
	}

	// We may not have to download the document
	// If coming from a crawl, this will be empty
	if (m_docInfo.getType().empty() == true)
	{
		m_docInfo.setType(MIMEScanner::scanUrl(thisUrl));
	}

	if (Dijon::FilterFactory::isSupportedType(m_docInfo.getType()) == false)
	{
		// Skip unsupported types ?
		if (m_allowAllMIMETypes == false)
		{
			m_status = _("Cannot index document type");
			m_status += " ";
			m_status += m_docInfo.getType();
			m_status += " ";
			m_status += _("at");
			m_status += " ";
			m_status += m_docInfo.getLocation();
			return;
		}
	}
	else
	{
		Dijon::Filter *pFilter = Dijon::FilterFactory::getFilter(m_docInfo.getType());

		if (pFilter != NULL)
		{
			// We may able to feed the document directly to the filter
			if (((pFilter->is_data_input_ok(Dijon::Filter::DOCUMENT_FILE_NAME) == true) &&
				(thisUrl.getProtocol() == "file")) ||
				((pFilter->is_data_input_ok(Dijon::Filter::DOCUMENT_URI) == true) &&
				(thisUrl.getProtocol() == "http")))
			{
				doDownload = false;
			}

			delete pFilter;
		}
	}

	if (doDownload == true)
	{
		DownloadingThread::doWork();
	}
	else
	{
		m_pDoc = new Document(m_docInfo);

		m_pDoc->setTimestamp(m_docInfo.getTimestamp());
		m_pDoc->setSize(m_docInfo.getSize());
#ifdef DEBUG
		cout << "IndexingThread::doWork: skipped download of " << m_docInfo.getLocation() << endl;
#endif
	}

	if (m_pDoc != NULL)
	{
		string docType(m_pDoc->getType());
		bool success = false;

		// The type may have been obtained when downloading
		if (docType.empty() == false)
		{
			m_docInfo.setType(docType);
		}
		else
		{
			m_pDoc->setType(m_docInfo.getType());
		}

		if (m_docInfo.getTitle().empty() == false)
		{
			// Use the title we were supplied with
			m_pDoc->setTitle(m_docInfo.getTitle());
		}
		else
		{
			// Use the document's
			m_docInfo.setTitle(m_pDoc->getTitle());
		}
#ifdef DEBUG
		cout << "IndexingThread::doWork: title is " << m_pDoc->getTitle() << endl;
#endif

		// Check again as the downloader may have altered the MIME type
		if (Dijon::FilterFactory::isSupportedType(m_docInfo.getType()) == false)
		{
			// Skip unsupported types ?
			if (m_allowAllMIMETypes == false)
			{
				m_status = _("Cannot index document type");
				m_status += " ";
				m_status += m_docInfo.getType();
				m_status += " ";
				m_status += _("at");
				m_status += " ";
				m_status += m_docInfo.getLocation();
				return;
			}

			// Let FilterWrapper handle unspported documents
		}
		else if ((PinotSettings::getInstance().m_ignoreRobotsDirectives == false) &&
			(m_docInfo.getType().length() >= 9) &&
			(m_docInfo.getType().substr(9) == "text/html"))
		{
			Dijon::HtmlFilter htmlFilter(m_docInfo.getType());

			if ((FilterUtils::feedFilter(*m_pDoc, &htmlFilter) == true) &&
				(htmlFilter.next_document() == true))
			{
				const map<string, string> &metaData = htmlFilter.get_meta_data();

				// See if the document has a ROBOTS META tag
				map<string, string>::const_iterator robotsIter = metaData.find("robots");
				if (robotsIter != metaData.end())
				{
					string robotsDirectives(robotsIter->second);
	
					// Is indexing allowed ?
					string::size_type pos1 = robotsDirectives.find("none");
					string::size_type pos2 = robotsDirectives.find("noindex");
					if ((pos1 != string::npos) ||
						(pos2 != string::npos))
					{
						// No, it isn't
						m_status = _("Robots META tag forbids indexing");
						m_status += " ";
						m_status += m_docInfo.getLocation();
						return;
					}
				}
			}
#ifdef DEBUG
			else cout << "IndexingThread::doWork: couldn't check document for ROBOTS directive" << endl;
#endif
		}

		if (m_done == false)
		{
			FilterWrapper wrapFilter(pIndex);

			// Update an existing document or add to the index ?
			if (m_update == true)
			{
				// Update the document
				if (wrapFilter.updateDocument(*m_pDoc, m_docId) == true)
				{
#ifdef DEBUG
					cout << "IndexingThread::doWork: updated " << m_pDoc->getLocation()
						<< " at " << m_docId << endl;
#endif
					success = true;
				}
#ifdef DEBUG
				else cout << "IndexingThread::doWork: couldn't update " << m_pDoc->getLocation() << endl;
#endif
			}
			else
			{
				const set<string> &labels = m_docInfo.getLabels();
				unsigned int docId = 0;

				// Index the document
				success = wrapFilter.indexDocument(*m_pDoc, labels, docId);
				if (success == true)
				{
					m_docId = docId;
#ifdef DEBUG
					cout << "IndexingThread::doWork: indexed " << m_pDoc->getLocation()
						<< " to " << m_docId << endl;
#endif
				}
#ifdef DEBUG
				else cout << "IndexingThread::doWork: couldn't index " << m_pDoc->getLocation() << endl;
#endif
			}

			if (success == false)
			{
				m_status = _("Couldn't index");
				m_status += " ";
				m_status += m_docInfo.getLocation();
			}
			else
			{
				// Flush the index ?
				if (m_immediateFlush == true)
				{
					pIndex->flush();
				}

				// The document properties may have changed
				pIndex->getDocumentInfo(m_docId, m_docInfo);
			}
		}
	}
#ifdef DEBUG
	else cout << "IndexingThread::doWork: couldn't download " << m_docInfo.getLocation() << endl;
#endif

	delete pIndex;
}

UnindexingThread::UnindexingThread(const set<unsigned int> &docIdList) :
	WorkerThread(),
	m_indexLocation(PinotSettings::getInstance().m_docsIndexLocation),
	m_docsCount(0)
{
	copy(docIdList.begin(), docIdList.end(), inserter(m_docIdList, m_docIdList.begin()));
}

UnindexingThread::UnindexingThread(const set<string> &labelNames, const string &indexLocation) :
	WorkerThread(),
	m_indexLocation(indexLocation),
	m_docsCount(0)
{
	copy(labelNames.begin(), labelNames.end(), inserter(m_labelNames, m_labelNames.begin()));
	if (indexLocation.empty() == true)
	{
		m_indexLocation = PinotSettings::getInstance().m_docsIndexLocation;
	}
}

UnindexingThread::~UnindexingThread()
{
}

string UnindexingThread::getType(void) const
{
	return "UnindexingThread";
}

unsigned int UnindexingThread::getDocumentsCount(void) const
{
	return m_docsCount;
}

bool UnindexingThread::stop(void)
{
	m_done = true;
	return true;
}

void UnindexingThread::doWork(void)
{
	IndexInterface *pIndex = PinotSettings::getInstance().getIndex(m_indexLocation);

	if ((pIndex == NULL) ||
		(pIndex->isGood() == false))
	{
		m_status = _("Index error on");
		m_status += " ";
		m_status += m_indexLocation;
		if (pIndex != NULL)
		{
			delete pIndex;
		}
		return;
	}

	// Be pessimistic and assume something will go wrong ;-)
	m_status = _("Couldn't unindex document(s)");

	// Are we supposed to remove documents based on labels ?
	if (m_docIdList.empty() == true)
	{
		// Yep
		// FIXME: better delete documents one label at a time
		for (set<string>::iterator iter = m_labelNames.begin(); iter != m_labelNames.end(); ++iter)
		{
			string labelName = (*iter);

			// By unindexing all documents that match the label,
			// we effectively delete the label from the index
			if (pIndex->unindexDocuments(labelName, IndexInterface::BY_LABEL) == true)
			{
#ifdef DEBUG
				cout << "UnindexingThread::doWork: removed label " << labelName << endl;
#endif
				// OK
				++m_docsCount;
			}
#ifdef DEBUG
			else cout << "UnindexingThread::doWork: couldn't remove label " << labelName << endl;
#endif
		}

		// Nothing to report
		m_status = "";
	}
	else
	{
		for (set<unsigned int>::iterator iter = m_docIdList.begin(); iter != m_docIdList.end(); ++iter)
		{
			unsigned int docId = (*iter);

			if (pIndex->unindexDocument(docId) == true)
			{
#ifdef DEBUG
				cout << "UnindexingThread::doWork: removed " << docId << endl;
#endif
				// OK
				++m_docsCount;
			}
#ifdef DEBUG
			else cout << "UnindexingThread::doWork: couldn't remove " << docId << endl;
#endif
		}
#ifdef DEBUG
		cout << "UnindexingThread::doWork: removed " << m_docsCount << " documents" << endl;
#endif
	}

	if (m_docsCount > 0)
	{
		// Flush the index ?
		if (m_immediateFlush == true)
		{
			pIndex->flush();
		}

		// Nothing to report
		m_status = "";
	}

	delete pIndex;
}

UpdateDocumentThread::UpdateDocumentThread(const string &indexName,
	unsigned int docId, const DocumentInfo &docInfo) :
	WorkerThread(),
	m_indexName(indexName),
	m_docId(docId),
	m_docInfo(docInfo)
{
}

UpdateDocumentThread::~UpdateDocumentThread()
{
}

string UpdateDocumentThread::getType(void) const
{
	return "UpdateDocumentThread";
}

unsigned int UpdateDocumentThread::getDocumentID(void) const
{
	return m_docId;
}

const DocumentInfo &UpdateDocumentThread::getDocumentInfo(void) const
{
	return m_docInfo;
}

bool UpdateDocumentThread::stop(void)
{
	m_done = true;
	return true;
}

void UpdateDocumentThread::doWork(void)
{
	if (m_done == false)
	{
		const map<string, string> &indexesMap = PinotSettings::getInstance().getIndexes();
		map<string, string>::const_iterator mapIter = indexesMap.find(m_indexName);
		if (mapIter == indexesMap.end())
		{
			m_status = _("Index");
			m_status += " ";
			m_status += m_indexName;
			m_status += " ";
			m_status += _("doesn't exist");
			return;
		}

		// Get the index at that location
		IndexInterface *pIndex = PinotSettings::getInstance().getIndex(mapIter->second);
		if ((pIndex == NULL) ||
			(pIndex->isGood() == false))
		{
			m_status = _("Index error on");
			m_status += " ";
			m_status += mapIter->second;
			if (pIndex != NULL)
			{
				delete pIndex;
			}
			return;
		}

		if (pIndex->updateDocumentInfo(m_docId, m_docInfo) == false)
		{
			m_status = _("Couldn't update document");
		}
		else
		{
			// Flush the index ?
			if (m_immediateFlush == true)
			{
				pIndex->flush();
			}

			// The document properties may have changed
			pIndex->getDocumentInfo(m_docId, m_docInfo);
		}

		delete pIndex;
	}
}

StartDaemonThread::StartDaemonThread() :
	WorkerThread()
{
}

StartDaemonThread::~StartDaemonThread()
{
}

string StartDaemonThread::getType(void) const
{
	return "StartDaemonThread";
}

bool StartDaemonThread::stop(void)
{
	m_done = true;
	return true;
}

void StartDaemonThread::doWork(void)
{
	if (m_done == false)
        {
		// Ask the daemon to reload its configuration
		// Let D-Bus activate the service if necessary
		DBusXapianIndex::reload();
	}
}

