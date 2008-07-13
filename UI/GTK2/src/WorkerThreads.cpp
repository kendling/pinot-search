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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <exception>
#include <iostream>
#include <fstream>
#include <glibmm/miscutils.h>
#include <glibmm/exception.h>

#include "config.h"
#include "NLS.h"
#include "Languages.h"
#include "MIMEScanner.h"
#include "StringManip.h"
#include "TimeConverter.h"
#include "Url.h"
#include "HtmlFilter.h"
#include "FilterUtils.h"
#include "ActionQueue.h"
#include "CrawlHistory.h"
#include "QueryHistory.h"
#include "DownloaderFactory.h"
#include "FilterWrapper.h"
#include "DBusIndex.h"
#include "ModuleFactory.h"
#include "WebEngine.h"
#include "PinotSettings.h"
#include "WorkerThreads.h"

using namespace Glib;
using namespace std;

// A function object to stop threads with for_each()
struct StopThreadFunc
{
public:
	void operator()(map<unsigned int, WorkerThread *>::value_type &p)
	{
		p.second->stop();
#ifdef DEBUG
		cout << "StopThreadFunc: stopped thread " << p.second->getId() << endl;
#endif
		Thread::yield();
	}
};

Dispatcher WorkerThread::m_dispatcher;
pthread_mutex_t WorkerThread::m_dispatcherMutex = PTHREAD_MUTEX_INITIALIZER;
bool WorkerThread::m_immediateFlush = true;

string WorkerThread::errorToString(int errorNum)
{
	if (errorNum == 0)
	{
		return "";
	}

	if (errorNum < INDEX_ERROR)
	{
		return strerror(errorNum);
	}

	// Internal error codes
	switch (errorNum)
	{
		case INDEX_ERROR:
			return _("Index error");
		case INDEXING_FAILED:
			return _("Couldn't index document");
		case UPDATE_FAILED:
			return _("Couldn't update document");
		case UNINDEXING_FAILED:
			return _("Couldn't unindex document(s)");
		case QUERY_FAILED:
			return _("Couldn't run query on search engine");
		case HISTORY_FAILED:
			return _("Couldn't get history for search engine");
		case DOWNLOAD_FAILED:
			return _("Couldn't retrieve document");
		case MONITORING_FAILED:
			return _("File monitor error");
		case OPENDIR_FAILED:
			return _("Couldn't open directory");
		case UNKNOWN_INDEX:
			return _("Index doesn't exist");
		case UNKNOWN_ENGINE:
			return  _("Couldn't create search engine");
		case UNSUPPORTED_TYPE:
			return _("Cannot index document type");
		case UNSUPPORTED_PROTOCOL:
			return _("No downloader for this protocol");
		case ROBOTS_FORBIDDEN:
			return _("Robots META tag forbids indexing");
		case NO_MONITORING:
			return _("No monitoring handler");
		default:
			break;
	}

	return _("Unknown error");
}

Dispatcher &WorkerThread::getDispatcher(void)
{
	return m_dispatcher;
}

void WorkerThread::immediateFlush(bool doFlush)
{
	m_immediateFlush = doFlush;
}

WorkerThread::WorkerThread() :
	m_startTime(time(NULL)),
	m_id(0),
	m_background(false),
	m_stopped(false),
	m_done(false),
	m_errorNum(0)
{
}

WorkerThread::~WorkerThread()
{
}

time_t WorkerThread::getStartTime(void) const
{
	return m_startTime;
}

void WorkerThread::setId(unsigned int id)
{
	m_id = id;
}

unsigned int WorkerThread::getId(void) const
{
	return m_id;
}

void WorkerThread::inBackground(void)
{
	m_background = true;
}

bool WorkerThread::isBackground(void) const
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
	return Thread::create(sigc::mem_fun(*this, &WorkerThread::threadHandler), false);
}

void WorkerThread::stop(void)
{
	m_stopped = m_done = true;
}

bool WorkerThread::isStopped(void) const
{
	return m_stopped;
}

bool WorkerThread::isDone(void) const
{
	return m_done;
}

int WorkerThread::getErrorNum(void) const
{
	return m_errorNum;
}

string WorkerThread::getStatus(void) const
{
	string status(errorToString(m_errorNum));

	if ((status.empty() == false) &&
		(m_errorParam.empty() == false))
	{
		status += " (";
		status += m_errorParam;
		status += ")";
	}

	return status;
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
	catch (Glib::Exception &ex)
	{
		cerr << "Glib exception in thread " << m_id << ", type " << getType()
			<< ":" << ex.what() << endl;
		m_errorNum = UNKNOWN_ERROR;
	}
	catch (std::exception &ex)
	{
		cerr << "STL exception in thread " << m_id << ", type " << getType()
			<< ":" << ex.what() << endl;
		m_errorNum = UNKNOWN_ERROR;
	}
	catch (...)
	{
		cerr << "Unknown exception in thread " << m_id << ", type " << getType() << endl;
		m_errorNum = UNKNOWN_ERROR;
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
	m_numCPUs(1),
	m_stopIndexing(false)
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
	time_t timeNow = time(NULL);
	WorkerThread *pWorkerThread = NULL;

	// Get the first thread that's finished
	if (write_lock_threads() == true)
	{
		for (map<unsigned int, WorkerThread *>::iterator threadIter = m_threads.begin();
			threadIter != m_threads.end(); ++threadIter)
		{
			unsigned int threadId = threadIter->first;

			if (threadIter->second->isDone() == false)
			{
#ifdef DEBUG
				cout << "ThreadsManager::get_thread: thread "
					<< threadId << " is not done" << endl;
#endif

				// Foreground threads ought not to run very long
				if ((threadIter->second->isBackground() == false) &&
					(threadIter->second->getStartTime() + 300 < timeNow))
				{
					// This thread has been running for more than 5 minutes already !
					threadIter->second->stop();

					cerr << "Stopped long-running thread " << threadId << endl;
				}
			}
			else
			{
				// This one will do...
				pWorkerThread = threadIter->second;
				// Remove it
				m_threads.erase(threadIter);
#ifdef DEBUG
				cout << "ThreadsManager::get_thread: thread " << threadId
					<< " is done, " << m_threads.size() << " left" << endl;
#endif
				break;
			}
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

	if (m_stopIndexing == true)
	{
#ifdef DEBUG
		cout << "ThreadsManager::index_document: stopped indexing" << endl;
#endif
		return _("Indexing was stopped");
	}

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
	bool createdThread = false;

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
#ifdef DEBUG
	else cout << "ThreadsManager::start_thread: thread " << pWorkerThread->getId()
			<< " will run in the foreground" << endl;
#endif

	// Insert
	pair<map<unsigned int, WorkerThread *>::iterator, bool> threadPair;
	if (write_lock_threads() == true)
	{
		threadPair = m_threads.insert(pair<unsigned int, WorkerThread *>(pWorkerThread->getId(), pWorkerThread));
		if (threadPair.second == false)
		{
			delete pWorkerThread;
			pWorkerThread = NULL;
		}

		unlock_threads();
	}

	// Start the thread
	if (pWorkerThread != NULL)
	{
		Thread *pThread = pWorkerThread->start();
		if (pThread != NULL)
		{
			createdThread = true;
		}
		else
		{
			// Erase
			if (write_lock_threads() == true)
			{
				m_threads.erase(threadPair.first);

				unlock_threads();
			}
			delete pWorkerThread;
		}
	}

	++m_nextThreadId;

	return createdThread;
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
	cout << "ThreadsManager::get_threads_count: " << count << "/"
		<< m_backgroundThreadsCount << " threads left" << endl;
#endif

	// A negative count would mean that a background thread
	// exited without signaling
	return (unsigned int)max(count , 0);
}

void ThreadsManager::stop_threads(void)
{
	if (m_threads.empty() == false)
	{
		if (write_lock_threads() == true)
		{
			// Stop threads
			for_each(m_threads.begin(), m_threads.end(), StopThreadFunc());

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
		sigc::mem_fun(*this, &ThreadsManager::on_thread_signal));
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

void IndexBrowserThread::doWork(void)
{
	set<unsigned int> docIDList;
	set<string> docLabels;
	unsigned int numDocs = 0;

	const map<string, string> &indexesMap = PinotSettings::getInstance().getIndexes();
	map<string, string>::const_iterator mapIter = indexesMap.find(m_indexName);
	if (mapIter == indexesMap.end())
	{
		m_errorNum = UNKNOWN_INDEX;
		m_errorParam = m_indexName;
		return;
	}

	// Get the index at that location
	IndexInterface *pIndex = PinotSettings::getInstance().getIndex(mapIter->second);
	if ((pIndex == NULL) ||
		(pIndex->isGood() == false))
	{
		m_errorNum = INDEX_ERROR;
		m_errorParam = mapIter->second;
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

	unsigned int indexId = PinotSettings::getInstance().getIndexIdByName(m_indexName);
	for (set<unsigned int>::iterator iter = docIDList.begin(); iter != docIDList.end(); ++iter)
	{
		unsigned int docId = (*iter);

		if (m_done == true)
		{
			break;
		}

		DocumentInfo docInfo;
		if (pIndex->getDocumentInfo(docId, docInfo) == true)
		{
			string type(docInfo.getType());

			if (type.empty() == true)
			{
				docInfo.setType("text/html");
			}
			docInfo.setIsIndexed(indexId, docId);

			// Insert that document
			m_documentsList.push_back(docInfo);
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
	m_correctedSpelling(false),
	m_isLive(true)
{
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

bool QueryingThread::isLive(void) const
{
	return m_isLive;
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

EngineQueryThread::EngineQueryThread(const string &engineName, const string &engineDisplayableName,
	const string &engineOption, const QueryProperties &queryProps,
	unsigned int startDoc, bool listingIndex) :
	QueryingThread(engineName, engineDisplayableName, engineOption, queryProps, startDoc, listingIndex)
{
#ifdef DEBUG
	cout << "EngineQueryThread::EngineQueryThread: engine " << m_engineName << ", " << m_engineOption
		<< ", mode " << m_listingIndex << endl;
#endif
}

EngineQueryThread::EngineQueryThread(const string &engineName, const string &engineDisplayableName,
	const string &engineOption, const QueryProperties &queryProps,
	const set<string> &limitToDocsSet, unsigned int startDoc) :
	QueryingThread(engineName, engineDisplayableName, engineOption, queryProps, startDoc, false)
{
	copy(limitToDocsSet.begin(), limitToDocsSet.end(),
		inserter(m_limitToDocsSet, m_limitToDocsSet.begin()));
#ifdef DEBUG
	cout << "EngineQueryThread::EngineQueryThread: engine " << m_engineName << ", " << m_engineOption
		<< ", limited to " << m_limitToDocsSet.size() << " documents" << endl;
#endif
}

EngineQueryThread::~EngineQueryThread()
{
}

void EngineQueryThread::processResults(const vector<DocumentInfo> &resultsList)
{
	PinotSettings &settings = PinotSettings::getInstance();
	IndexInterface *pDocsIndex = NULL;
	IndexInterface *pDaemonIndex = NULL;
	unsigned int indexId = 0;
	bool isIndexQuery = false;

	// Are we querying an index ?
	if (ModuleFactory::isSupported(m_engineName, true) == true)
	{
		// Internal index ?
		if ((m_engineOption == settings.m_docsIndexLocation) ||
			(m_engineOption == settings.m_daemonIndexLocation))
		{
			indexId = settings.getIndexIdByLocation(m_engineOption);
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
		DocumentInfo currentDoc(*resultIter);
		string title(_("No title"));
		string location(currentDoc.getLocation());
		string language(currentDoc.getLanguage());
		unsigned int docId = 0;

		// The title may contain formatting
		if (currentDoc.getTitle().empty() == false)
		{
			title = FilterUtils::stripMarkup(currentDoc.getTitle());
		}
		currentDoc.setTitle(title);
#ifdef DEBUG
		cout << "EngineQueryThread::processResults: title is " << title << endl;
#endif

		// Use the query's language if the result's is unknown
		if (language.empty() == true)
		{
			language = m_queryProps.getStemmingLanguage();
		}
		currentDoc.setLanguage(language);

		if (isIndexQuery == true)
		{
			unsigned int tmpId = 0;

			// The index engine should have set this
			docId = currentDoc.getIsIndexed(tmpId);
		}

		// Is this in one of the indexes ?
		if ((pDocsIndex != NULL) &&
			(pDocsIndex->isGood() == true))
		{
			docId = pDocsIndex->hasDocument(location);
			if (docId > 0)
			{
				indexId = settings.getIndexIdByName(_("My Web Pages"));
			}
		}
		if ((pDaemonIndex != NULL) &&
			(pDaemonIndex->isGood() == true) &&
			(docId == 0))
		{
			docId = pDaemonIndex->hasDocument(location);
			if (docId > 0)
			{
				indexId = settings.getIndexIdByName(_("My Documents"));
			}
		}

		if (docId > 0)
		{
			currentDoc.setIsIndexed(indexId, docId);
#ifdef DEBUG
			cout << "EngineQueryThread::processResults: found in index " << indexId << endl;
#endif
		}
#ifdef DEBUG
		else cout << "EngineQueryThread::processResults: not found in any index" << endl;
#endif

		m_documentsList.push_back(currentDoc);
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

void EngineQueryThread::processResults(const vector<DocumentInfo> &resultsList,
	unsigned int indexId)
{
	unsigned int zeroId = 0;

	// Copy the results list
	for (vector<DocumentInfo>::const_iterator resultIter = resultsList.begin();
		resultIter != resultsList.end(); ++resultIter)
	{
		DocumentInfo currentDoc(*resultIter);

		// The engine has no notion of index IDs
		unsigned int docId = currentDoc.getIsIndexed(zeroId);
		currentDoc.setIsIndexed(indexId, docId);

		m_documentsList.push_back(currentDoc);
	}
}

void EngineQueryThread::doWork(void)
{
	PinotSettings &settings = PinotSettings::getInstance();

	// Get the SearchEngine
	SearchEngineInterface *pEngine = ModuleFactory::getSearchEngine(m_engineName, m_engineOption);
	if (pEngine == NULL)
	{
		m_errorNum = UNKNOWN_ENGINE;
		m_errorParam = m_engineDisplayableName;
		return;
	}

	// Set up the proxy
	WebEngine *pWebEngine = dynamic_cast<WebEngine *>(pEngine);
	if (pWebEngine != NULL)
	{
		DownloaderInterface *pDownloader = pWebEngine->getDownloader();
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

		pWebEngine->setEditableValues(settings.m_editablePluginValues);
	}

	if (m_listingIndex == false)
	{
		pEngine->setLimitSet(m_limitToDocsSet);
	}

	// Run the query
	pEngine->setDefaultOperator(SearchEngineInterface::DEFAULT_OP_AND);
	if (pEngine->runQuery(m_queryProps, m_startDoc) == false)
	{
		m_errorNum = QUERY_FAILED;
		m_errorParam = m_engineDisplayableName;
	}
	else
	{
		const vector<DocumentInfo> &resultsList = pEngine->getResults();

		m_documentsList.clear();
		m_documentsList.reserve(resultsList.size());
		m_documentsCount = pEngine->getResultsCountEstimate();
#ifdef DEBUG
		cout << "EngineQueryThread::doWork: " << resultsList.size() << " off " << m_documentsCount
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
				PinotSettings::getInstance().getIndexIdByName(m_engineDisplayableName));
		}

		// Don't spellcheck if the query was modified in any way
		if (m_queryProps.getModified() == false)
		{
			string correctedFreeQuery(pEngine->getSpellingCorrection());

			// Any spelling correction ?
			if (correctedFreeQuery.empty() == false)
			{
				m_correctedSpelling = true;
				m_queryProps.setFreeQuery(correctedFreeQuery);
			}
		}
	}

	delete pEngine;
}

EngineHistoryThread::EngineHistoryThread(const string &engineDisplayableName,
	const QueryProperties &queryProps, unsigned int maxDocsCount) :
	QueryingThread("", engineDisplayableName, "", queryProps, 0, false),
	m_maxDocsCount(maxDocsCount)
{
#ifdef DEBUG
	cout << "EngineHistoryThread::EngineHistoryThread: engine " << m_engineDisplayableName << endl;
#endif
	// Results are converted to UTF-8 prior to insertion in the history database
	m_resultsCharset = "UTF-8";
	m_isLive = false;
}

EngineHistoryThread::~EngineHistoryThread()
{
}

void EngineHistoryThread::doWork(void)
{
	QueryHistory queryHistory(PinotSettings::getInstance().getHistoryDatabaseName());

	if (queryHistory.getItems(m_queryProps.getName(), m_engineDisplayableName,
		m_maxDocsCount, m_documentsList) == false)
	{
		m_errorNum = HISTORY_FAILED;
		m_errorParam = m_engineDisplayableName;
	}
	else if (m_documentsList.empty() == false)
	{
		// Get the first result's charset
		queryHistory.getItemExtract(m_queryProps.getName(), m_engineDisplayableName,
			m_documentsList.front().getLocation());
	}
}

ExpandQueryThread::ExpandQueryThread(const QueryProperties &queryProps,
	const set<string> &expandFromDocsSet) :
	WorkerThread(),
	m_queryProps(queryProps)
{
	copy(expandFromDocsSet.begin(), expandFromDocsSet.end(),
		inserter(m_expandFromDocsSet, m_expandFromDocsSet.begin()));
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

void ExpandQueryThread::doWork(void)
{
	// Get the SearchEngine
	SearchEngineInterface *pEngine = ModuleFactory::getSearchEngine(PinotSettings::getInstance().m_defaultBackend, "MERGED");
	if (pEngine == NULL)
	{
		m_errorNum = UNKNOWN_ENGINE;
		m_errorParam = m_queryProps.getName();
		return;
	}

	// Expand the query
	pEngine->setExpandSet(m_expandFromDocsSet);

	// Run the query
	pEngine->setDefaultOperator(SearchEngineInterface::DEFAULT_OP_AND);
	if (pEngine->runQuery(m_queryProps) == false)
	{
		m_errorNum = QUERY_FAILED;
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

LabelUpdateThread::LabelUpdateThread(const set<string> &labelsToAdd,
	const set<string> &labelsToDelete, const map<string, string> &labelsToRename) :
	WorkerThread(),
	m_resetLabels(false)
{
	copy(labelsToAdd.begin(), labelsToAdd.end(),
		inserter(m_labelsToAdd, m_labelsToAdd.begin()));
	copy(labelsToDelete.begin(), labelsToDelete.end(),
		inserter(m_labelsToDelete, m_labelsToDelete.begin()));
	copy(labelsToRename.begin(), labelsToRename.end(),
		inserter(m_labelsToRename, m_labelsToRename.begin()));
}

LabelUpdateThread::LabelUpdateThread(const set<string> &labelsToAdd,
	const set<unsigned int> &docsIds, const set<unsigned int> &daemonIds,
	bool resetLabels) :
	WorkerThread(),
	m_resetLabels(resetLabels)
{
	copy(labelsToAdd.begin(), labelsToAdd.end(),
		inserter(m_labelsToAdd, m_labelsToAdd.begin()));
	copy(docsIds.begin(), docsIds.end(),
		inserter(m_docsIds, m_docsIds.begin()));
	copy(daemonIds.begin(), daemonIds.end(),
		inserter(m_daemonIds, m_daemonIds.begin()));
}

LabelUpdateThread::~LabelUpdateThread()
{
}

string LabelUpdateThread::getType(void) const
{
	return "LabelUpdateThread";
}

bool LabelUpdateThread::modifiedDocsIndex(void) const
{
	return !m_docsIds.empty();
}

bool LabelUpdateThread::modifiedDaemonIndex(void) const
{
	return !m_daemonIds.empty();
}

void LabelUpdateThread::doWork(void)
{
	bool actOnDocuments = false;

	IndexInterface *pDocsIndex = PinotSettings::getInstance().getIndex(PinotSettings::getInstance().m_docsIndexLocation);
	if (pDocsIndex == NULL)
	{
		m_errorNum = INDEX_ERROR;
		m_errorParam = PinotSettings::getInstance().m_docsIndexLocation;
		return;
	}

	IndexInterface *pDaemonIndex = PinotSettings::getInstance().getIndex(PinotSettings::getInstance().m_daemonIndexLocation);
	if (pDaemonIndex == NULL)
	{
		m_errorNum = INDEX_ERROR;
		m_errorParam = PinotSettings::getInstance().m_daemonIndexLocation;
		delete pDocsIndex;
		return;
	}

	// Apply the labels to existing documents
	if (m_docsIds.empty() == false)
	{
		pDocsIndex->setDocumentsLabels(m_docsIds, m_labelsToAdd, m_resetLabels);
		actOnDocuments = true;
	}
	if (m_daemonIds.empty() == false)
	{
		pDaemonIndex->setDocumentsLabels(m_daemonIds, m_labelsToAdd, m_resetLabels);
		actOnDocuments = true;
	}

	if (actOnDocuments == false)
	{
		// Add labels
		for (set<string>::iterator iter = m_labelsToAdd.begin(); iter != m_labelsToAdd.end(); ++iter)
		{
			pDocsIndex->addLabel(*iter);
			pDaemonIndex->addLabel(*iter);
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
		m_errorNum = UNSUPPORTED_PROTOCOL;
		m_errorParam = thisUrl.getProtocol();
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
		m_errorNum = DOWNLOAD_FAILED;
		m_errorParam = m_docInfo.getLocation();
	}
}

IndexingThread::IndexingThread(const DocumentInfo &docInfo, const string &indexLocation,
	bool allowAllMIMETypes) :
	DownloadingThread(docInfo),
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

void IndexingThread::doWork(void)
{
	IndexInterface *pIndex = PinotSettings::getInstance().getIndex(m_indexLocation);
	Url thisUrl(m_docInfo.getLocation());
	bool doDownload = true;

	// First things first, get the index
	if ((pIndex == NULL) ||
		(pIndex->isGood() == false))
	{
		m_errorNum = INDEX_ERROR;
		m_errorParam = m_indexLocation;
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

	if (FilterUtils::isSupportedType(m_docInfo.getType()) == false)
	{
		// Skip unsupported types ?
		if (m_allowAllMIMETypes == false)
		{
			m_errorNum = UNSUPPORTED_TYPE;
			m_errorParam = m_docInfo.getType();
			delete pIndex;

			return;
		}
	}
	else
	{
		Dijon::Filter *pFilter = FilterUtils::getFilter(m_docInfo.getType());

		if (pFilter != NULL)
		{
			// We may able to feed the document directly to the filter
			if (((pFilter->is_data_input_ok(Dijon::Filter::DOCUMENT_FILE_NAME) == true) &&
				(thisUrl.getProtocol() == "file")) ||
				((pFilter->is_data_input_ok(Dijon::Filter::DOCUMENT_URI) == true) &&
				(thisUrl.isLocal() == false)))
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
		if (FilterUtils::isSupportedType(m_docInfo.getType()) == false)
		{
			// Skip unsupported types ?
			if (m_allowAllMIMETypes == false)
			{
				m_errorNum = UNSUPPORTED_TYPE;
				m_errorParam = m_docInfo.getType();
				delete pIndex;

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
						m_errorNum = ROBOTS_FORBIDDEN;
						m_errorParam = m_docInfo.getLocation();
						delete pIndex;

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
				unsigned int docId = 0;
#ifdef DEBUG
				cout << "IndexingThread::doWork: " << m_docInfo.getLabels().size()
					<< " labels for URL " << m_pDoc->getLocation() << endl;
#endif

				// Index the document
				success = wrapFilter.indexDocument(*m_pDoc, m_docInfo.getLabels(), docId);
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
				m_errorNum = INDEXING_FAILED;
				m_errorParam = m_docInfo.getLocation();
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
				m_docInfo.setIsIndexed(PinotSettings::getInstance().getIndexIdByLocation(m_indexLocation), m_docId);
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

void UnindexingThread::doWork(void)
{
	IndexInterface *pIndex = PinotSettings::getInstance().getIndex(m_indexLocation);

	if ((pIndex == NULL) ||
		(pIndex->isGood() == false))
	{
		m_errorNum = INDEX_ERROR;
		m_errorParam = m_indexLocation;
		if (pIndex != NULL)
		{
			delete pIndex;
		}
		return;
	}

	// Be pessimistic and assume something will go wrong ;-)
	m_errorNum = UNINDEXING_FAILED;

	// Are we supposed to remove documents based on labels ?
	if (m_docIdList.empty() == true)
	{
		// Yep, delete documents one label at a time
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
		m_errorNum = 0;
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
		m_errorNum = 0;
	}

	delete pIndex;
}

UpdateDocumentThread::UpdateDocumentThread(const string &indexName, unsigned int docId,
	const DocumentInfo &docInfo, bool updateLabels) :
	WorkerThread(),
	m_indexName(indexName),
	m_docId(docId),
	m_docInfo(docInfo),
	m_updateLabels(updateLabels)
{
}

UpdateDocumentThread::~UpdateDocumentThread()
{
}

string UpdateDocumentThread::getType(void) const
{
	return "UpdateDocumentThread";
}

string UpdateDocumentThread::getIndexName(void) const
{
	return m_indexName;
}

unsigned int UpdateDocumentThread::getDocumentID(void) const
{
	return m_docId;
}

const DocumentInfo &UpdateDocumentThread::getDocumentInfo(void) const
{
	return m_docInfo;
}

void UpdateDocumentThread::doWork(void)
{
	if (m_done == false)
	{
		const map<string, string> &indexesMap = PinotSettings::getInstance().getIndexes();
		map<string, string>::const_iterator mapIter = indexesMap.find(m_indexName);
		if (mapIter == indexesMap.end())
		{
			m_errorNum = UNKNOWN_INDEX;
			m_errorParam = m_indexName;
			return;
		}

		// Get the index at that location
		IndexInterface *pIndex = PinotSettings::getInstance().getIndex(mapIter->second);
		if ((pIndex == NULL) ||
			(pIndex->isGood() == false))
		{
			m_errorNum = INDEX_ERROR;
			m_errorParam = mapIter->second;
			if (pIndex != NULL)
			{
				delete pIndex;
			}
			return;
		}

		// Update the DocumentInfo
		if (pIndex->updateDocumentInfo(m_docId, m_docInfo) == false)
		{
			m_errorNum = UPDATE_FAILED;
			m_errorParam = m_docInfo.getLocation();
			return;
		}
		// ...and the labels if necessary
		if (m_updateLabels == true)
		{
			if (pIndex->setDocumentLabels(m_docId, m_docInfo.getLabels()) == false)
			{
				m_errorNum = UPDATE_FAILED;
				m_errorParam = m_docInfo.getLocation();
				return;
			}
		}

		// Flush the index ?
		if (m_immediateFlush == true)
		{
			pIndex->flush();
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

void StartDaemonThread::doWork(void)
{
	// Ask the daemon to reload its configuration
	// Let D-Bus activate the service if necessary
	DBusIndex::reload();
}

MonitorThread::MonitorThread(MonitorInterface *pMonitor, MonitorHandler *pHandler,
	bool checkHistory) :
	WorkerThread(),
	m_ctrlReadPipe(-1),
	m_ctrlWritePipe(-1),
	m_pMonitor(pMonitor),
	m_pHandler(pHandler),
	m_checkHistory(checkHistory)
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

void MonitorThread::stop(void)
{
	WorkerThread::stop();
	write(m_ctrlWritePipe, "X", 1);
}

void MonitorThread::processEvents(void)
{
	CrawlHistory crawlHistory(PinotSettings::getInstance().getHistoryDatabaseName());
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

		// Skip dotfiles and blacklisted files
		Url urlObj("file://" + event.m_location);
		if ((urlObj.getFile()[0] == '.') ||
			(PinotSettings::getInstance().isBlackListed(event.m_location) == true))
		{
			// Next
			events.pop();
			continue;
		}

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
				m_pHandler->directoryCreated(event.m_location);
			}
		}
		else if (event.m_type == MonitorEvent::WRITE_CLOSED)
		{
			if (event.m_isDirectory == false)
			{
				CrawlHistory::CrawlStatus status = CrawlHistory::UNKNOWN;
				struct stat fileStat;
				time_t itemDate = 0;

				if (m_checkHistory == false)
				{
					m_pHandler->fileModified(event.m_location);
				}
				else if (crawlHistory.hasItem("file://" + event.m_location, status, itemDate) == true)
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
		m_errorNum = NO_MONITORING;
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
			m_errorNum = MONITORING_FAILED;
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

