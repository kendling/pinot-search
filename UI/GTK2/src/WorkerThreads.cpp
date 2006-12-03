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

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <exception>
#include <iostream>
#include <fstream>
#include <sigc++/class_slot.h>
#include <sigc++/compatibility.h>
#include <sigc++/slot.h>
#include <glibmm/miscutils.h>

#include "HtmlTokenizer.h"
#include "XmlTokenizer.h"
#include "MIMEScanner.h"
#include "TokenizerFactory.h"
#include "StringManip.h"
#include "TimeConverter.h"
#include "Url.h"
#include "XapianDatabase.h"
#include "ActionQueue.h"
#include "CrawlHistory.h"
#include "QueryHistory.h"
#include "IndexedDocument.h"
#include "DownloaderFactory.h"
#include "SearchEngineFactory.h"
#include "IndexFactory.h"
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

// A function object to delete threads with for_each()
struct DeleteThreadFunc
{
public:
	void operator()(map<WorkerThread *, Thread *>::value_type &p)
	{
#ifdef DEBUG
		cout << "DeleteThreadFunc: waiting for thread " << p.first->getId() << endl;
#endif
		// FIXME: the documentation says resources of the thread, including the Thread object
		// are released by join()
		p.second->join();
		delete p.first;
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
	m_joinable(true),
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
	return Thread::create(slot_class(*this, &WorkerThread::threadHandler), m_joinable);
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
#ifdef DEBUG
		cout << "WorkerThread::emitSignal: signaled end of thread " << m_id << endl;
#endif

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
				threadIter->second->join();
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

	// Is it an update ?
	IndexInterface *pIndex = PinotSettings::getInstance().getIndex(m_defaultIndexLocation);
	if (pIndex == NULL)
	{
		ustring status = _("Index error on");
		status += " ";
		status += m_defaultIndexLocation;
		return status;
	}

	unsigned int docId = pIndex->hasDocument(docInfo.getLocation());
	if (docId > 0)
	{
		// Yes, it is
		start_thread(new IndexingThread(docInfo, docId, m_defaultIndexLocation));
	}
	else
	{
		// This is a new document
		start_thread(new IndexingThread(docInfo, docId, m_defaultIndexLocation));
	}
	delete pIndex;

	return "";
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
			// Join them
			for_each(m_threads.begin(), m_threads.end(), DeleteThreadFunc());
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
		ActionQueue queue(PinotSettings::getInstance().m_historyDatabase, get_application_name());

		queue.pushItem(ActionQueue::INDEX, docInfo);

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

		// Get an item ?
		if (getItem == true)
		{
			ActionQueue queue(PinotSettings::getInstance().m_historyDatabase, get_application_name());
			ActionQueue::ActionType type;
			DocumentInfo docInfo;

			while (queue.popItem(type, docInfo) == true)
			{
				if (type != ActionQueue::INDEX)
				{
					continue;
				}

				ustring status = index_document(docInfo);
				if (status.empty() == true)
				{
					foundItem = true;
					break;
				}
			}
		}

		unlock_lists();
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

IndexBrowserThread::IndexBrowserThread(const string &indexName,
	const string &labelName, unsigned int maxDocsCount, unsigned int startDoc) :
	WorkerThread(),
	m_indexName(indexName),
	m_labelName(labelName),
	m_indexDocsCount(0),
	m_maxDocsCount(maxDocsCount),
	m_startDoc(startDoc)
{
}

IndexBrowserThread::~IndexBrowserThread()
{
}

string IndexBrowserThread::getType(void) const
{
	return "IndexBrowserThread";
}

string IndexBrowserThread::getIndexName(void) const
{
	return m_indexName;
}

string IndexBrowserThread::getLabelName(void) const
{
	return m_labelName;
}

unsigned int IndexBrowserThread::getDocumentsCount(void) const
{
	return m_indexDocsCount;
}

bool IndexBrowserThread::stop(void)
{
	// Disconnect the signal
	Signal3<void, IndexedDocument, unsigned int, string>::slot_list_type slotsList = m_signalUpdate.slots();
	Signal3<void, IndexedDocument, unsigned int, string>::slot_list_type::iterator slotIter = slotsList.begin();
	if (slotIter != slotsList.end())
	{
		if (slotIter->empty() == false)
		{
			slotIter->block();
			slotIter->disconnect();
		}
	}
	m_done = true;

	return true;
}

SigC::Signal3<void, IndexedDocument, unsigned int, string>& IndexBrowserThread::getUpdateSignal(void)
{
	return m_signalUpdate;
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

	m_indexDocsCount = pIndex->getDocumentsCount(m_labelName);
	if (m_indexDocsCount == 0)
	{
#ifdef DEBUG
		cout << "IndexBrowserThread::doWork: no documents" << endl;
#endif
		return;
	}

#ifdef DEBUG
	cout << "IndexBrowserThread::doWork: " << m_maxDocsCount << " off " << m_indexDocsCount
		<< " documents to browse, starting at " << m_startDoc << endl;
#endif
	if (m_labelName.empty() == true)
	{
		pIndex->listDocuments(docIDList, m_maxDocsCount, m_startDoc);
	}
	else
	{
		pIndex->listDocumentsWithLabel(m_labelName, docIDList, m_maxDocsCount, m_startDoc);
	}
	for (set<unsigned int>::iterator iter = docIDList.begin(); iter != docIDList.end(); ++iter)
	{
		if (m_done == true)
		{
			break;
		}

		// Get the document ID
		unsigned int docId = (*iter);
		// ...and the document URL
		string url = XapianDatabase::buildUrl(mapIter->second, docId);

		DocumentInfo docInfo;
		if (pIndex->getDocumentInfo(docId, docInfo) == true)
		{
			string type = docInfo.getType();
			if (type.empty() == true)
			{
				type = "text/html";
			}

			IndexedDocument indexedDoc(docInfo.getTitle(), url, docInfo.getLocation(),
				type, docInfo.getLanguage());
			indexedDoc.setTimestamp(docInfo.getTimestamp());
			indexedDoc.setSize(docInfo.getSize());

			// Signal
			m_signalUpdate(indexedDoc, docId, m_indexName);
			++numDocs;
		}
#ifdef DEBUG
		else cout << "IndexBrowserThread::doWork: couldn't retrieve document " << docId << endl;
#endif
	}
	delete pIndex;
}

QueryingThread::QueryingThread(const string &engineName, const string &engineDisplayableName,
	const string &engineOption, const QueryProperties &queryProps) :
	WorkerThread(),
	m_engineName(engineName),
	m_engineDisplayableName(engineDisplayableName),
	m_engineOption(engineOption),
	m_queryProps(queryProps)
{
}

QueryingThread::~QueryingThread()
{
}

string QueryingThread::getType(void) const
{
	return "QueryingThread";
}

string QueryingThread::getEngineName(void) const
{
	return m_engineDisplayableName;
}

QueryProperties QueryingThread::getQuery(void) const
{
	return m_queryProps;
}

const vector<Result> &QueryingThread::getResults(string &charset) const
{
	charset = m_resultsCharset;
#ifdef DEBUG
	cout << "QueryingThread::getResults: charset for " << m_engineDisplayableName << " is " << charset << endl;
#endif

	return m_resultsList;
}

bool QueryingThread::stop(void)
{
	m_done = true;
	return true;
}

void QueryingThread::doWork(void)
{
	// Get the SearchEngine
	SearchEngineInterface *pEngine = SearchEngineFactory::getSearchEngine(m_engineName, m_engineOption);
	if (pEngine == NULL)
	{
		m_status = _("Couldn't create search engine");
		m_status += " ";
		m_status += m_engineDisplayableName;
		return;
	}

	// Set the maximum number of results
	pEngine->setMaxResultsCount(m_queryProps.getMaximumResultsCount());

	// Run the query
	if (pEngine->runQuery(m_queryProps) == false)
	{
		m_status = _("Couldn't run query on search engine");
		m_status += " ";
		m_status += m_engineDisplayableName;
	}
	else
	{
		const vector<Result> &resultsList = pEngine->getResults();

		m_resultsList.clear();
		m_resultsList.reserve(resultsList.size());
		m_resultsCharset = pEngine->getResultsCharset();

		// Copy the results list
		for (vector<Result>::const_iterator resultIter = resultsList.begin();
			resultIter != resultsList.end(); ++resultIter)
		{
			string title(_("No title"));
			string language(resultIter->getLanguage());

			// The title may contain formatting
			if (resultIter->getTitle().empty() == false)
			{
				title = XmlTokenizer::stripTags(resultIter->getTitle());
			}
#ifdef DEBUG
			cout << "QueryingThread::doWork: title is " << title << endl;
#endif

			// Use the query's language if the result's is unknown
			if (language.empty() == true)
			{
				language = m_queryProps.getLanguage();
			}

			m_resultsList.push_back(Result(resultIter->getLocation(),
				title,
				resultIter->getExtract(),
				language,
				resultIter->getScore()));
		}
	}

	delete pEngine;
}

ExpandQueryThread::ExpandQueryThread(const string &engineName,
	const string &engineOption, const QueryProperties &queryProps,
	const set<unsigned int> &relevantDocs) :
	WorkerThread(),
	m_engineName(engineName),
	m_engineOption(engineOption),
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
	// Get the SearchEngine
	SearchEngineInterface *pEngine = SearchEngineFactory::getSearchEngine(m_engineName, m_engineOption);
	if (pEngine == NULL)
	{
		m_status = _("Couldn't create search engine");
		m_status += " ";
		m_status += m_queryProps.getName();
		return;
	}

	// Set the maximum number of results
	pEngine->setMaxResultsCount(m_queryProps.getMaximumResultsCount());
	// Set whether to expand the query
	pEngine->setQueryExpansion(m_relevantDocs);

	// Run the query
	if (pEngine->runQuery(m_queryProps) == false)
	{
		m_status = _("Couldn't run query on search engine");
		m_status += " ";
		m_status += m_engineName;
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
	const map<string, string> &labelsToRename)
{
	copy(labelsToDelete.begin(), labelsToDelete.end(), inserter(m_labelsToDelete, m_labelsToDelete.begin()));
	copy(labelsToRename.begin(), labelsToRename.end(), inserter(m_labelsToRename, m_labelsToRename.begin()));
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
		m_pDoc = m_pDownloader->retrieveUrl(m_docInfo);
	}

	if (m_pDoc == NULL)
	{
		m_status = _("Couldn't retrieve");
		m_status += " ";
		m_status += m_docInfo.getLocation();
	}
}

IndexingThread::IndexingThread(const DocumentInfo &docInfo, unsigned int docId,
	const string &indexLocation, bool allowAllMIMETypes) :
	DownloadingThread(docInfo),
	m_docInfo(docInfo),
	m_docId(docId),
	m_indexLocation(indexLocation),
	m_allowAllMIMETypes(allowAllMIMETypes)
{
	if (m_docId > 0)
	{
		// Ignore robots directives on updates
		m_ignoreRobotsDirectives = true;
		m_update = true;
	}
	else
	{
		m_ignoreRobotsDirectives = PinotSettings::getInstance().m_ignoreRobotsDirectives;
		// This is not an update
		m_update = false;
	}
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
	Tokenizer::DataNeeds dataNeeds;
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

	// We may not have to download the document
	if (thisUrl.getProtocol() == "file")
	{
		// If coming from a crawl, this will be empty
		if (m_docInfo.getType().empty() == true)
		{
			m_docInfo.setType(MIMEScanner::scanFile(m_docInfo.getLocation()));
		}

		if (TokenizerFactory::isSupportedType(m_docInfo.getType(), dataNeeds) == false)
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
		else if (dataNeeds == Tokenizer::ALL_BUT_FILES)
		{
			doDownload = false;
		}
	}

	if (doDownload == true)
	{
		DownloadingThread::doWork();
#ifdef DEBUG
		cout << "IndexingThread::doWork: downloaded " << m_docInfo.getLocation() << endl;
#endif
	}
	else
	{
		m_pDoc = new Document(m_docInfo);
#ifdef DEBUG
		cout << "IndexingThread::doWork: skipped download of " << m_docInfo.getLocation() << endl;
#endif
	}

	if (m_pDoc != NULL)
	{
		Tokenizer *pTokens = NULL;
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
		if (TokenizerFactory::isSupportedType(m_docInfo.getType(), dataNeeds) == false)
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
#ifdef DEBUG
			cout << "IndexingThread::doWork: can't index document content" << endl;
#endif

			// Create an empty document so that the file's details are indexed but not its content
			delete m_pDoc;
			m_pDoc = new Document(m_docInfo);
			// A simple text tokenizer will do the job
			pTokens = new Tokenizer(m_pDoc);
		}
		else
		{
			pTokens = TokenizerFactory::getTokenizerByType(m_docInfo.getType(), m_pDoc);
		}

		if (pTokens == NULL)
                {
                        m_status = _("Couldn't tokenize");
                        m_status += " ";
                        m_status += m_docInfo.getLocation();
                        return;
                }

		// Is indexing allowed ?
		HtmlTokenizer *pHtmlTokens = dynamic_cast<HtmlTokenizer*>(pTokens);
		if ((m_ignoreRobotsDirectives == false) &&
			(pHtmlTokens != NULL))
		{
			// See if the document has a ROBOTS META tag
			string robotsDirectives = pHtmlTokens->getMetaTag("robots");
			string::size_type pos1 = robotsDirectives.find("none");
			string::size_type pos2 = robotsDirectives.find("noindex");
			if ((pos1 != string::npos) ||
				(pos2 != string::npos))
			{
				// No, it's not
				delete pTokens;
				m_status = _("Robots META tag forbids indexing");
				m_status += " ";
				m_status += m_docInfo.getLocation();
				return;
			}
		}

		if (m_done == false)
		{
			pIndex->setStemmingMode(IndexInterface::STORE_BOTH);

			// Update an existing document or add to the index ?
			if (m_update == true)
			{
				// Update the document
				if (pIndex->updateDocument(m_docId, *pTokens) == true)
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
				success = pIndex->indexDocument(*pTokens, labels, docId);
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

		delete pTokens;
	}

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
			if (pIndex->unindexDocuments(labelName) == true)
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

MonitorThread::MonitorThread(MonitorInterface *pMonitor, MonitorHandler *pHandler) :
	WorkerThread(),
	m_ctrlReadPipe(-1),
	m_ctrlWritePipe(-1),
	m_pMonitor(pMonitor),
	m_pHandler(pHandler)
{
	int pipeFds[2];

	if (pipe(pipeFds) == 0)
	{
		// This pipe will allow to stop select()
		m_ctrlReadPipe = pipeFds[0];
		m_ctrlWritePipe = pipeFds[1];
	}
}

MonitorThread::~MonitorThread()
{
	close(m_ctrlReadPipe);
	close(m_ctrlWritePipe);
}

string MonitorThread::getType(void) const
{
	return "MonitorThread";
}

bool MonitorThread::stop(void)
{
	// Disconnect the signal
	Signal3<void, const DocumentInfo&, const std::string&, bool>::slot_list_type slotsList = m_signalDirectoryFound.slots();
	Signal3<void, const DocumentInfo&, const std::string&, bool>::slot_list_type::iterator slotIter = slotsList.begin();
	if (slotIter != slotsList.end())
	{
		if (slotIter->empty() == false)
		{
			slotIter->block();
			slotIter->disconnect();
		}
	}
	m_done = true;
	write(m_ctrlWritePipe, "X", 1);

	return true;
}

Signal3<void, const DocumentInfo&, const std::string&, bool>& MonitorThread::getDirectoryFoundSignal(void)
{
	return m_signalDirectoryFound;
}

void MonitorThread::processEvents(void)
{
	CrawlHistory history(PinotSettings::getInstance().m_historyDatabase);
	queue<MonitorEvent> events;

#ifdef DEBUG
	cout << "MonitorThread::processEvents: checking for events" << endl;
#endif
	if ((m_pMonitor == NULL) ||
		(m_pMonitor->retrievePendingEvents(events) == false))
	{
#ifdef DEBUG
		cout << "MonitorThread::processEvents: failed to retrieve pending events" << endl;
#endif
		return;
	}
#ifdef DEBUG
	cout << "MonitorThread::processEvents: retrieved " << events.size() << " events" << endl;
#endif

	while ((events.empty() == false) &&
		(m_done == false))
	{
		MonitorEvent &event = events.front();

		if ((event.m_location.empty() == true) ||
			(event.m_type == MonitorEvent::UNKNOWN))
		{
			// Next
			events.pop();
			continue;
		}
#ifdef DEBUG
		cout << "MonitorThread::processEvents: event " << event.m_type << " on "
			<< event.m_location << " " << event.m_isDirectory << endl;
#endif

		// What's the event code ?
		if (event.m_type == MonitorEvent::EXISTS)
		{
			if (event.m_isDirectory == false)
			{
				m_pHandler->fileExists(event.m_location);
			}
		}
		else if (event.m_type == MonitorEvent::CREATED)
		{
			if (event.m_isDirectory == false)
			{
				m_pHandler->fileCreated(event.m_location);
			}
			else
			{
				DocumentInfo docInfo("", string("file://") + event.m_location, "", "");

				// Report this directory so that it is crawled
				m_signalDirectoryFound(docInfo, "", true);
			}
		}
		else if (event.m_type == MonitorEvent::WRITE_CLOSED)
		{
			if (event.m_isDirectory == false)
			{
				CrawlHistory::CrawlStatus status = CrawlHistory::UNKNOWN;
				struct stat fileStat;
				time_t itemDate;

				if (history.hasItem("file://" + event.m_location, status, itemDate) == true)
				{
					// Was the file actually modified ?
					if ((stat(event.m_location.c_str(), &fileStat) == 0) &&
						(itemDate < fileStat.st_mtime))
					{
						m_pHandler->fileModified(event.m_location);
					}
#ifdef DEBUG
					else cout << "MonitorThread::processEvents: file wasn't modified" << endl;
#endif
				}
#ifdef DEBUG
				else cout << "MonitorThread::processEvents: file wasn't crawled" << endl;
#endif
			}
		}
		else if (event.m_type == MonitorEvent::MOVED)
		{
			if (event.m_isDirectory == false)
			{
				m_pHandler->fileMoved(event.m_location, event.m_previousLocation);
			}
			else
			{
				// We should receive this only if the destination directory is monitored too
				m_pHandler->directoryMoved(event.m_location, event.m_previousLocation);
			}
		}
		else if (event.m_type == MonitorEvent::DELETED)
		{
			if (event.m_isDirectory == false)
			{
				m_pHandler->fileDeleted(event.m_location);
			}
			else
			{
				// The monitor should have stopped monitoring this
				// In practice, events for the files in this directory will already have been received 
				m_pHandler->directoryDeleted(event.m_location);
			}
		}

		// Next
		events.pop();
	}
}

void MonitorThread::doWork(void)
{
	if ((m_pHandler == NULL) ||
		(m_pMonitor == NULL))
	{
		m_status = _("No monitoring handler");
		return;
	}

	// Initialize the handler
	m_pHandler->initialize();

	// Get the list of files to monitor
	const set<string> &fileNames = m_pHandler->getFileNames();
	for (set<string>::const_iterator fileIter = fileNames.begin();
		fileIter != fileNames.end(); ++fileIter)
	{
		m_pMonitor->addLocation(*fileIter, false);
	}
	// Directories, if any, are set elsewhere
	// In the case of OnDiskHandler, they are set by DirectoryScannerThread

	// There might already be events that need processing
	processEvents();

	// Wait for something to happen
	while (m_done == false)
	{
		struct timeval selectTimeout;
		fd_set listenSet;

		selectTimeout.tv_sec = 60;
		selectTimeout.tv_usec = 0;

		FD_ZERO(&listenSet);
		if (m_ctrlReadPipe >= 0)
		{
			FD_SET(m_ctrlReadPipe, &listenSet);
		}

		m_pHandler->flushIndex();

		// The file descriptor may change over time
		int monitorFd = m_pMonitor->getFileDescriptor();
		FD_SET(monitorFd, &listenSet);
		if (monitorFd < 0)
		{
			m_status = _("Couldn't initialize file monitor");
			return;
		}

		int fdCount = select(max(monitorFd, m_ctrlReadPipe) + 1, &listenSet, NULL, NULL, &selectTimeout);
		if ((fdCount < 0) &&
			(errno != EINTR))
		{
#ifdef DEBUG
			cout << "MonitorThread::doWork: select() failed" << endl;
#endif
			break;
		}
		else if (FD_ISSET(monitorFd, &listenSet))
		{
			processEvents();
		}
	}
}

DirectoryScannerThread::DirectoryScannerThread(const string &dirName, bool isSource,
	unsigned int maxLevel, bool followSymLinks,
	MonitorInterface *pMonitor, MonitorHandler *pHandler) :
	WorkerThread(),
	m_dirName(dirName),
	m_maxLevel(maxLevel),
	m_followSymLinks(followSymLinks),
	m_pMonitor(pMonitor),
	m_pHandler(pHandler),
	m_currentLevel(0),
	m_sourceId(0)
{
	if (m_dirName.empty() == false)
	{
		CrawlHistory history(PinotSettings::getInstance().m_historyDatabase);

		if (isSource == true)
		{
			// Does this source exist ?
			if (history.hasSource("file://" + m_dirName, m_sourceId) == false)
			{
				// Create it
				m_sourceId = history.insertSource("file://" + m_dirName);
			}
		}
		else
		{
			map<unsigned int, string> fileSources;

			// What source does this belong to ?
			for(map<unsigned int, string>::const_iterator sourceIter = fileSources.begin();
				sourceIter != fileSources.end(); ++sourceIter)
			{
				if (sourceIter->second.length() < m_dirName.length())
				{
					// Skip
					continue;
				}

				if (sourceIter->second.substr(0, m_dirName.length()) == m_dirName)
				{
					// That's the one
					m_sourceId = sourceIter->first;
					break;
				}
			}
		}
	}
}

DirectoryScannerThread::~DirectoryScannerThread()
{
}

string DirectoryScannerThread::getType(void) const
{
	return "DirectoryScannerThread";
}

string DirectoryScannerThread::getDirectory(void) const
{
	return m_dirName;
}

bool DirectoryScannerThread::stop(void)
{
	// Disconnect the signal
	Signal3<void, const DocumentInfo&, const std::string&, bool>::slot_list_type slotsList = m_signalFileFound.slots();
	Signal3<void, const DocumentInfo&, const std::string&, bool>::slot_list_type::iterator slotIter = slotsList.begin();
	if (slotIter != slotsList.end())
	{
		if (slotIter->empty() == false)
		{
			slotIter->block();
			slotIter->disconnect();
		}
	}
	m_done = true;

	return true;
}

Signal3<void, const DocumentInfo&, const std::string&, bool>& DirectoryScannerThread::getFileFoundSignal(void)
{
	return m_signalFileFound;
}

void DirectoryScannerThread::foundFile(const DocumentInfo &docInfo)
{
	char labelStr[64];

	if ((docInfo.getLocation().empty() == true) ||
		(m_done == true))
	{
		return;
	}

	// This identifies the source
	snprintf(labelStr, 64, "SOURCE%u", m_sourceId);

	m_signalFileFound(docInfo, labelStr, false);
}

bool DirectoryScannerThread::scanEntry(const string &entryName)
{
	CrawlHistory history(PinotSettings::getInstance().m_historyDatabase);
	CrawlHistory::CrawlStatus status = CrawlHistory::UNKNOWN;
	time_t itemDate;
	struct stat fileStat;
	int statSuccess = 0;

	if (entryName.empty() == true)
	{
		return false;
	}

	// Skip . .. and dotfiles
	Url urlObj("file://" + entryName);
	if (urlObj.getFile()[0] == '.')
	{
		return false;
	}

	if (m_followSymLinks == false)
	{
		statSuccess = lstat(entryName.c_str(), &fileStat);
	}
	else
	{
		// Stat the files pointed to by symlinks
		statSuccess = stat(entryName.c_str(), &fileStat);
	}

	if (statSuccess == -1)
	{
		return false;
	}

	bool itemExists = history.hasItem("file://" + entryName, status, itemDate);

	// Is it a file or a directory ?
	if (S_ISLNK(fileStat.st_mode))
	{
		// This won't happen when m_followSymLinks is true
		return false;
	}
	else if (S_ISREG(fileStat.st_mode))
	{
		DocumentInfo docInfo;
		bool reportFile = false;

		docInfo.setLocation("file://" + entryName);

		// Is this file blacklisted ?
		// We have to check early so that if necessary the file's status stays at CRAWLING 
		// and it is removed from the index at the end of this crawl
		if (PinotSettings::getInstance().isBlackListed(entryName) == false)
		{
			if (itemExists == false)
			{
				// Record it
				history.insertItem(docInfo.getLocation(), CrawlHistory::CRAWLED, m_sourceId, fileStat.st_mtime);
#ifdef DEBUG
				cout << "DirectoryScannerThread::scanEntry: reporting new file " << entryName << endl;
#endif
				reportFile = true;
			}
			else
			{
				// Update the record
				history.updateItem(docInfo.getLocation(), CrawlHistory::CRAWLED, fileStat.st_mtime);

				// Was it last crawled after it was modified ?
				if (itemDate < fileStat.st_mtime)
				{
#ifdef DEBUG
					cout << "DirectoryScannerThread::scanEntry: reporting modified file " << entryName << endl;
#endif
					// No, crawl and index it again
					reportFile = true;
				}
			}
		}

		if (reportFile == true)
		{
			Url urlObj(docInfo.getLocation());

			docInfo.setTitle(urlObj.getFile());
			docInfo.setTimestamp(TimeConverter::toTimestamp(fileStat.st_mtime));
			docInfo.setSize(fileStat.st_size);

			foundFile(docInfo);
		}
	}
	else if (S_ISDIR(fileStat.st_mode))
	{
		// Can we scan this directory ?
		if (((m_maxLevel == 0) ||
			(m_currentLevel < m_maxLevel)) &&
			(PinotSettings::getInstance().isBlackListed(entryName) == false))
		{
			++m_currentLevel;

			// Open the directory
			DIR *pDir = opendir(entryName.c_str());
			if (pDir == NULL)
			{
				return false;
			}
#ifdef DEBUG
			cout << "DirectoryScannerThread::scanEntry: entering " << entryName << endl;
#endif

			if (m_pMonitor != NULL)
			{
				// Monitor first so that we don't miss events
				m_pMonitor->addLocation(entryName, true);
			}

			// Iterate through this directory's entries
			struct dirent *pDirEntry = readdir(pDir);
			while ((m_done == false) &&
				(pDirEntry != NULL))
			{
				char *pEntryName = pDirEntry->d_name;

				// Skip . .. and dotfiles
				if ((pEntryName != NULL) &&
					(pEntryName[0] != '.'))
				{
					string subEntryName(entryName);

					if (entryName[entryName.length() - 1] != '/')
					{
						subEntryName += "/";
					}
					subEntryName += pEntryName;

					// Scan this entry
					if (scanEntry(subEntryName) == false)
					{
#ifdef DEBUG
						cout << "DirectoryScannerThread::scanEntry: failed to open "
							<< subEntryName << endl;
#endif
					}
				}

				// Next entry
				pDirEntry = readdir(pDir);
			}
#ifdef DEBUG
			cout << "DirectoryScannerThread::scanEntry: done with " << entryName << endl;
#endif

			// Close the directory
			closedir(pDir);
			--m_currentLevel;
		}
	}
	else
	{
		return false;
	}

	return true;
}

void DirectoryScannerThread::doWork(void)
{
	CrawlHistory history(PinotSettings::getInstance().m_historyDatabase);
	set<string> deletedFiles;

	if (m_dirName.empty() == true)
	{
		return;
	}

	// Update this source's items status
	history.updateItemsStatus(m_sourceId, CrawlHistory::CRAWLED, CrawlHistory::CRAWLING);

	if (scanEntry(m_dirName) == false)
	{
		m_status = _("Couldn't open directory");
		m_status += " ";
		m_status += m_dirName;
	}

	if (m_done == true)
	{
#ifdef DEBUG
		cout << "DirectoryScannerThread::doWork: leaving cleanup until next crawl" << endl;
#endif
		return;
	}

	// All files with status set to CRAWLING were not found in this crawl
	// Chances are they were removed after the previous crawl
	if ((m_pHandler != NULL) &&
		(history.getSourceItems(m_sourceId, CrawlHistory::CRAWLING, deletedFiles) > 0))
	{
#ifdef DEBUG
		cout << "DirectoryScannerThread::doWork: " << deletedFiles.size() << " files were deleted" << endl;
#endif
		for(set<string>::const_iterator fileIter = deletedFiles.begin();
			fileIter != deletedFiles.end(); ++fileIter)
		{
			// Inform the MonitorHandler
			if (m_pHandler->fileDeleted(fileIter->substr(7)) == true)
			{
				// Delete this item
				history.deleteItem(*fileIter);
			}
		}
	}
#ifdef DEBUG
	cout << "DirectoryScannerThread::doWork: done crawling " << m_dirName << endl;
#endif
}

