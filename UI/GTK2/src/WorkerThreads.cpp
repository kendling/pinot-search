/*
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
#include <fam.h>
#include <exception>
#include <iostream>
#include <fstream>
#include <sigc++/class_slot.h>

#include "HtmlTokenizer.h"
#include "MIMEScanner.h"
#include "TokenizerFactory.h"
#include "StringManip.h"
#include "TimeConverter.h"
#include "Url.h"
#include "QueryHistory.h"
#include "IndexedDocument.h"
#include "DownloaderFactory.h"
#include "SearchEngineFactory.h"
#ifdef HAS_GOOGLEAPI
#include "GoogleAPIEngine.h"
#endif
#include "XapianIndex.h"
#include "XapianEngine.h"
#include "config.h"
#include "NLS.h"
#include "PinotSettings.h"
#include "WorkerThreads.h"

using namespace SigC;
using namespace Glib;
using namespace std;

// A function object to stop and delete threads with for_each()
struct DeleteMapPointer
{
public:
	void operator()(map<WorkerThread *, Thread *>::value_type &p)
	{
		p.first->stop();
#ifdef DEBUG
		cout << "DeleteMapPointer: waiting for thread " << p.first->getId() << endl;
#endif
		p.second->join();
		delete p.first;
	}
};

Dispatcher WorkerThread::m_dispatcher;

Dispatcher &WorkerThread::getDispatcher(void)
{
	return m_dispatcher;
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
	return Thread::create(slot_class(*this, &WorkerThread::threadHandler), m_joinable);
}

bool WorkerThread::isDone(void) const
{
	return m_done;
}

void WorkerThread::reset(void)
{
	m_done = false;
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
#ifdef DEBUG
		cout << "Exception in thread " << m_id << ", type " << getType()
			<< ":" << ex.what() << endl;
#endif
	}
	catch (...)
	{
#ifdef DEBUG
		cout << "Unknown exception in thread " << m_id << ", type " << getType() << endl;
#endif
	}

	emitSignal();
}

void WorkerThread::emitSignal(void)
{
#ifdef DEBUG
	cout << "WorkerThread::emitSignal: end of thread " << m_id << endl;
#endif
	m_done = true;
	m_dispatcher.emit();
}

ThreadsManager::ThreadsManager() :
	m_nextId(1),
	m_backgroundThreadsCount(0)
{
	pthread_rwlock_init(&m_rwLock, NULL);
}

ThreadsManager::~ThreadsManager()
{
	stop_threads();
	// Destroy the read/write lock
	pthread_rwlock_destroy(&m_rwLock);
}

bool ThreadsManager::read_lock(unsigned int where)
{
	if (pthread_rwlock_rdlock(&m_rwLock) == 0)
	{
#ifdef DEBUG
		cout << "ThreadsManager::read_lock " << where << endl;
#endif
		return true;
	}

	return false;
}

bool ThreadsManager::write_lock(unsigned int where)
{
	if (pthread_rwlock_wrlock(&m_rwLock) == 0)
	{
#ifdef DEBUG
		cout << "ThreadsManager::write_lock " << where << endl;
#endif
		return true;
	}

	return false;
}

void ThreadsManager::unlock(void)
{
#ifdef DEBUG
	cout << "ThreadsManager::unlock" << endl;
#endif
	pthread_rwlock_unlock(&m_rwLock);
}

WorkerThread *ThreadsManager::on_thread_end(void)
{
	WorkerThread *pWorkerThread = NULL;

	// Get the first thread that's finished
	if (read_lock(1) == true)
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
			cout << "ThreadsManager::on_thread_end: thread "
				<< threadIter->first->getId() << " is not done" << endl;
#endif
		}

		unlock();
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

bool ThreadsManager::start_thread(WorkerThread *pWorkerThread, bool inBackground)
{
	bool insertedThread = false;

	if (pWorkerThread == NULL)
	{
		return false;
	}

	pWorkerThread->setId(m_nextId);
	if (inBackground == true)
	{
		pWorkerThread->inBackground();
		++m_backgroundThreadsCount;
	}

	// Start the thread
	Thread *pThread = pWorkerThread->start();
	if (pThread == NULL)
	{
		return false;
	}

	// Insert
	if (write_lock(2) == true)
	{
		m_threads[pWorkerThread] = pThread;

		unlock();
	}
	++m_nextId;

	return true;
}

unsigned int ThreadsManager::get_threads_count(void)
{
	int count = 0;

	if (read_lock(3) == true)
	{
		count = m_threads.size() - m_backgroundThreadsCount;

		unlock();
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
		if (read_lock(4) == true)
		{
			for_each(m_threads.begin(), m_threads.end(), DeleteMapPointer());
			m_threads.clear();

			unlock();
		}
	}
}

void ThreadsManager::disconnect(void)
{
	m_threadsEndConnection.block();
	m_threadsEndConnection.disconnect();
#ifdef DEBUG
	cout << "ThreadsManager::disconnect: disconnected" << endl;
#endif
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
	m_done = true;
	m_status = _("Stopped browsing");
	m_status += " ";
	m_status += m_indexName;
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
	XapianIndex index(mapIter->second);
	if (index.isGood() == false)
	{
		m_status = _("Index error on");
		m_status += " ";
		m_status += mapIter->second;
		return;
	}

	m_indexDocsCount = index.getDocumentsCount();
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
		index.listDocuments(docIDList, m_maxDocsCount, m_startDoc);
	}
	else
	{
		index.listDocumentsWithLabel(m_labelName, docIDList, m_maxDocsCount, m_startDoc);
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
		string url = XapianEngine::buildUrl(mapIter->second, docId);

		DocumentInfo docInfo;
		if (index.getDocumentInfo(docId, docInfo) == true)
		{
			string type = docInfo.getType();
			if (type.empty() == true)
			{
				type = "text/html";
			}

			string date = docInfo.getTimestamp();
			IndexedDocument indexedDoc(docInfo.getTitle(), url, docInfo.getLocation(),
				type, docInfo.getLanguage());
			indexedDoc.setTimestamp(date);

			// Signal
			m_signalUpdate(indexedDoc, docId, m_indexName);
			++numDocs;
		}
#ifdef DEBUG
		else cout << "IndexBrowserThread::doWork: couldn't retrieve document " << docId << endl;
#endif
	}
}

QueryingThread::QueryingThread(const string &engineName, const string &engineDisplayableName,
	const string &engineOption, const QueryProperties &queryProps) :
	WorkerThread(),
	m_queryProps(queryProps),
	m_engineName(engineName),
	m_engineDisplayableName(engineDisplayableName),
	m_engineOption(engineOption)
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
	cout << "QueryingThread::getResults: charset is " << charset << endl;
#endif

	return m_resultsList;
}

bool QueryingThread::stop(void)
{
	m_done = true;
	m_status = _("Stopped querying");
	m_status += " ";
	m_status += m_engineDisplayableName;
	return true;
}

void QueryingThread::doWork(void)
{
	// Get the SearchEngine
	SearchEngineInterface *engine = SearchEngineFactory::getSearchEngine(m_engineName, m_engineOption);
	if (engine == NULL)
	{
		m_status = _("Couldn't create search engine");
		m_status += " ";
		m_status += m_engineDisplayableName;
		return;
	}
	// Set the maximum number of results
	engine->setMaxResultsCount(m_queryProps.getMaximumResultsCount());

	// Run the query
	if (engine->runQuery(m_queryProps) == false)
	{
		m_status = _("Couldn't run query on search engine");
		m_status += " ";
		m_status += m_engineDisplayableName;
	}
	else
	{
		const vector<Result> &resultsList = engine->getResults();

		m_resultsList.clear();
		m_resultsList.reserve(resultsList.size());
		m_resultsCharset = engine->getResultsCharset();

		// Copy the results list
		for (vector<Result>::const_iterator resultIter = resultsList.begin();
			resultIter != resultsList.end(); ++resultIter)
		{
			string title = _("No title");
			string extract = HtmlTokenizer::stripTags(resultIter->getExtract());

			// The title may contain formatting
			if (resultIter->getTitle().empty() == false)
			{
				title = HtmlTokenizer::stripTags(resultIter->getTitle());
			}

			string language = resultIter->getLanguage();
			if (language.empty() == true)
			{
				// Use the query's language
				language = m_queryProps.getLanguage();
			}

			m_resultsList.push_back(Result(resultIter->getLocation(),
				title,
				extract,
				language,
				resultIter->getScore()));
		}
	}
	delete engine;
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
	m_status = _("Stopped querying index labels");
	return true;
}

void LabelUpdateThread::doWork(void)
{
	XapianIndex docsIndex(PinotSettings::getInstance().m_indexLocation);
	if (docsIndex.isGood() == false)
	{
		m_status = _("Index error on");
		m_status += " ";
		m_status += PinotSettings::getInstance().m_indexLocation;
		return;
	}

	XapianIndex mailIndex(PinotSettings::getInstance().m_mailIndexLocation);
	if (mailIndex.isGood() == false)
	{
		m_status = _("Index error on");
		m_status += " ";
		m_status += PinotSettings::getInstance().m_mailIndexLocation;
		return;
	}

	// Delete labels
	for (set<string>::iterator iter = m_labelsToDelete.begin(); iter != m_labelsToDelete.end(); ++iter)
	{
		docsIndex.deleteLabel(*iter);
		mailIndex.deleteLabel(*iter);
	}
	// Rename labels
	for (map<string, string>::iterator iter = m_labelsToRename.begin(); iter != m_labelsToRename.end(); ++iter)
	{
		docsIndex.renameLabel(iter->first, iter->second);
		mailIndex.renameLabel(iter->first, iter->second);
	}
}

DownloadingThread::DownloadingThread(const string url, bool fromCache) :
	WorkerThread(),
	m_url(url),
	m_fromCache(fromCache),
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
	return m_url;
}

const Document *DownloadingThread::getDocument(void) const
{
	return m_pDoc;
}

bool DownloadingThread::stop(void)
{
	m_done = true;
	m_status = _("Stopped retrieval of");
	m_status += " ";
	m_status += m_url;

	return true;
}

void DownloadingThread::doWork(void)
{
	if (m_pDownloader != NULL)
	{
		delete m_pDownloader;
		m_pDownloader = NULL;
	}

	Url thisUrl(m_url);

	if (m_fromCache == true)
	{
#ifdef HAS_GOOGLEAPI
		GoogleAPIEngine googleApiEngine;
		googleApiEngine.setKey(PinotSettings::getInstance().m_googleAPIKey);
		m_pDoc = googleApiEngine.retrieveCachedUrl(m_url);
#endif
#ifdef DEBUG
		cout << "DownloadingThread::doWork: got cached page" << endl;
#endif
	}
	else
	{
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
			DocumentInfo docInfo("", m_url, "", "");

			m_pDoc = m_pDownloader->retrieveUrl(docInfo);
		}
	}

	if (m_pDoc == NULL)
	{
		m_status = _("Couldn't retrieve");
		m_status += " ";
		m_status += m_url;
	}
}

IndexingThread::IndexingThread(const DocumentInfo &docInfo, const string &labelName,
	unsigned int docId) :
	DownloadingThread(docInfo.getLocation(), false),
	m_docInfo(docInfo),
	m_labelName(labelName),
	m_docId(docId)
{
	m_indexLocation = PinotSettings::getInstance().m_indexLocation;
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

string IndexingThread::getLabelName(void) const
{
	return m_labelName;
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
		m_status = _("Stopped indexing");
		m_status += " ";
		m_status += m_url;
		return true;
	}

	return false;
}

void IndexingThread::doWork(void)
{
	// First things first, get the index
	XapianIndex index(m_indexLocation);
	if (index.isGood() == false)
	{
		m_status = _("Index error on");
		m_status += " ";
		m_status += m_indexLocation;
		return;
	}

	DownloadingThread::doWork();
#ifdef DEBUG
	cout << "IndexingThread::doWork: downloaded !" << endl;
#endif

	if (m_pDoc == NULL)
	{
		m_status = _("Couldn't retrieve");
		m_status += " ";
		m_status += m_url;
	}
	else
	{
		unsigned int urlContentLen;
		string docType = m_pDoc->getType();
		const char *urlContent = m_pDoc->getData(urlContentLen);
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

		// Skip unsupported types
		if (TokenizerFactory::isSupportedType(m_docInfo.getType()) == false)
		{
			m_status = _("Cannot index document type");
			m_status += " ";
			m_status += m_docInfo.getType();
			m_status += " ";
			m_status += _("at");
			m_status += " ";
			m_status += m_url;
			return;
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

		// Tokenize this document
		Tokenizer *pTokens = TokenizerFactory::getTokenizerByType(m_docInfo.getType(), m_pDoc);
		if (pTokens == NULL)
		{
			m_status = _("Couldn't tokenize");
			m_status += " ";
			m_status += m_url;
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
				m_status += m_url;
				return;
			}
		}

		if (m_done == false)
		{
			set<string> labels;

			index.setStemmingMode(IndexInterface::STORE_BOTH);

			// Update an existing document or add to the index ?
			if (m_update == true)
			{
				// Get the document's current labels
				index.getDocumentLabels(m_docId, labels);

				// Update the document
				if (index.updateDocument(m_docId, *pTokens) == true)
				{
					// Add the label if it's a new one
					if (labels.find(m_labelName) == labels.end())
					{
						labels.clear();
						labels.insert(m_labelName);

						index.setDocumentLabels(m_docId, labels, false);
					}
					success = true;
				}
			}
			else
			{
				unsigned int docId = 0;

				labels.insert(m_labelName);

				// Index the document
				success = index.indexDocument(*pTokens, labels, docId);
				if (success == true)
				{
					m_docId = docId;
				}
			}

			if (success == false)
			{
				m_status = _("Couldn't index");
				m_status += " ";
				m_status += m_url;
			}
			else if (m_done == false)
			{
				// Flush the index
				index.flush();

				// The document properties may have changed
				index.getDocumentInfo(m_docId, m_docInfo);
			}
		}

		delete pTokens;
	}
}

UnindexingThread::UnindexingThread(const set<unsigned int> &docIdList) :
	WorkerThread(),
	m_indexLocation(PinotSettings::getInstance().m_indexLocation),
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
		m_indexLocation = PinotSettings::getInstance().m_indexLocation;
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
	m_status = _("Stopped unindexing document(s)");
	return true;
}

void UnindexingThread::doWork(void)
{
	if (m_done == false)
	{
		XapianIndex index(m_indexLocation);

		if (index.isGood() == false)
		{
			m_status = _("Index error on");
			m_status += " ";
			m_status += m_indexLocation;
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
				if (index.unindexDocuments(labelName) == true)
				{
					// OK
					++m_docsCount;
				}
			}

			// Nothing to report
			m_status = "";
		}
		else
		{
			for (set<unsigned int>::iterator iter = m_docIdList.begin(); iter != m_docIdList.end(); ++iter)
			{
				unsigned int docId = (*iter);

				if (index.unindexDocument(docId) == true)
				{
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
			// Flush the index
			index.flush();

			// Nothing to report
			m_status = "";
		}
	}
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
	m_status = _("Stopped document update for");
	m_status += " ";
	m_status += m_docId;

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
		XapianIndex index(mapIter->second);
		if (index.isGood() == false)
		{
			m_status = _("Index error on");
			m_status += " ";
			m_status += mapIter->second;
			return;
		}

		if (index.updateDocumentInfo(m_docId, m_docInfo) == false)
		{
			m_status = _("Couldn't update document");
		}
		else
		{
			// Flush the index
			index.flush();
			// The document properties may have changed
			index.getDocumentInfo(m_docId, m_docInfo);
		}
	}
}

MonitorThread::MonitorThread(MonitorHandler *pHandler) :
	WorkerThread(),
	m_ctrlReadPipe(-1),
	m_ctrlWritePipe(-1),
	m_pHandler(pHandler),
	m_numCPUs(1)
{
	int pipeFds[2];

	if (pipe(pipeFds) == 0)
	{
		// This pipe will allow to stop select()
		m_ctrlReadPipe = pipeFds[0];
		m_ctrlWritePipe = pipeFds[1];
	}
	m_numCPUs = sysconf(_SC_NPROCESSORS_ONLN);
}

MonitorThread::~MonitorThread()
{
	// It's our responsability to delete the MonitorHandler object
	if (m_pHandler != NULL)
	{
		delete m_pHandler;
	}
	close(m_ctrlReadPipe);
	close(m_ctrlWritePipe);
}

string MonitorThread::getType(void) const
{
	return "MonitorThread";
}

bool MonitorThread::stop(void)
{
	m_done = true;
	write(m_ctrlWritePipe, "Stop", 4);
	m_status = _("Stopped monitoring");
#ifdef DEBUG
	cout << "MonitorThread::doWork: stop called" << endl;
#endif

	return true;
}

void MonitorThread::doWork(void)
{
	FAMConnection famConn;
	FAMRequest famReq;
	map<unsigned long, string> fsLocations;
	struct stat fileStat;
	bool setLocationsToMonitor = true;
	bool firstTime = true;
	bool resumeMonitor = false;
	int famStatus = -1;

	if (m_pHandler == NULL)
	{
		m_status = _("No monitoring handler");
		return;
	}

	// Wait for something to happen
	while (m_done == false)
	{
		struct timeval selectTimeout;
		fd_set listenSet;
		int famFd = -1;

		selectTimeout.tv_sec = 60;
		selectTimeout.tv_usec = 0;

		FD_ZERO(&listenSet);
		if (m_ctrlReadPipe >= 0)
		{
			FD_SET(m_ctrlReadPipe, &listenSet);
		}

		if ((setLocationsToMonitor == true) &&
			(m_pHandler->getFileSystemLocations(fsLocations) > 0) &&
			(m_pHandler->hasNewLocations() == true))
		{
			// Tell FAM what we want to monitor
#ifdef DEBUG
			cout << "MonitorThread::doWork: change detected" << endl;
#endif
			if (firstTime == false)
			{
				// Cancel
				FAMCancelMonitor(&famConn, &famReq);
				FAMClose(&famConn);
			}
			else
			{
				firstTime = false;
			}
			resumeMonitor = false;

			// FIXME: opening a new connection every time might be overkill
			if (FAMOpen(&famConn) != 0)
			{
				m_status = _("Couldn't open FAM connection");
				return;
			}

			// Go through the locations map
			for (map<unsigned long, string>::const_iterator fsIter = fsLocations.begin(); fsIter != fsLocations.end(); ++fsIter)
			{
				string fsLocation = fsIter->second;
				struct stat fileStat;

				if (m_done == true)
				{
					break;
				}
				if (stat(fsLocation.c_str(), &fileStat) == -1)
				{
					continue;
				}

				// Is that a file or a directory ?
				if (S_ISREG(fileStat.st_mode))
				{
					famStatus = FAMMonitorFile(&famConn, fsLocation.c_str(), &famReq, NULL);
				}
				else if (S_ISDIR(fileStat.st_mode))
				{
					// FIXME: FAM works one level deep only: monitor sub-directories if there are any...
					famStatus = FAMMonitorDirectory(&famConn, fsLocation.c_str(), &famReq, (void*)(fsIter->first + 1));
				}
#ifdef DEBUG
				cout << "MonitorThread::doWork: added " << fsLocation << ", " << famStatus << endl;
#endif
			}

			famFd = FAMCONNECTION_GETFD(&famConn);
			FD_SET(famFd, &listenSet);
		}
		setLocationsToMonitor = false;

		int fdCount = select(max(famFd, m_ctrlReadPipe) + 1, &listenSet, NULL, NULL, &selectTimeout);
		if ((fdCount < 0) &&
			(errno != EINTR))
		{
#ifdef DEBUG
			cout << "MonitorThread::doWork: select() failed" << endl;
#endif
			break;
		}
		else if ((famFd >= 0) &&
			(FD_ISSET(famFd, &listenSet)))
		{
			// There might be more than one event waiting...
			int pendingEvents = FAMPending(&famConn);
			if (pendingEvents < 0)
			{
#ifdef DEBUG
				cout << "MonitorThread::doWork: FAMPending() failed" << endl;
#endif
				break;
			}
			while ((pendingEvents >= 1) &&
				(m_done == false))
			{
				double averageLoad[3];

				// Get the load averaged over the last minute
				if (getloadavg(averageLoad, 3) != -1)
				{
					// FIXME: is LOADAVG_1MIN Solaris specific ?
					if (averageLoad[0] >= (double)m_numCPUs * 4)
					{
						// Ignore pending events if the load has become too high
#ifdef DEBUG
						cout << "MonitorThread::doWork: cancelling monitoring because of load (" << averageLoad[0] << ")" << endl;
#endif
						FAMCancelMonitor(&famConn, &famReq);
						resumeMonitor = true;
						break;
					}
				}

				FAMEvent famEvent;
				if ((FAMNextEvent(&famConn, &famEvent) == 1) &&
					(famEvent.filename != NULL) &&
					(strlen(famEvent.filename) > 0))
				{
					string fileName;
					bool updatedIndex = false;

#ifdef DEBUG
					cout << "MonitorThread::doWork: event " << famEvent.code
						<< " on " << famEvent.filename << endl;
#endif
					if (famEvent.code == FAMEndExist)
					{
						updatedIndex = m_pHandler->fileExists(famEvent.filename, true);
						// FIXME: accounts for which we didn't receive a FAMExists should
						// be removed
					}
					else
					{
						// Are we monitoring a file or a directory ?
						if (famEvent.userdata != NULL)
						{
							// A directory...
							if (famEvent.filename[0] == '/')
							{
								// Not interested in monitored directories...
								continue;
							}

							// The event is on a file in that directory
							map<unsigned long, string>::const_iterator fsIter = fsLocations.find((unsigned long)famEvent.userdata);
							if (fsIter == fsLocations.end())
							{
								continue;
							}
							fileName += fsIter->second;
							fileName += "/";
						}
						fileName += famEvent.filename;

						// What's the event code ?
						if (famEvent.code == FAMExists)
						{
							updatedIndex = m_pHandler->fileExists(fileName);
						}
						else if (famEvent.code == FAMCreated)
						{
							m_pHandler->fileCreated(fileName);
						}
						else if (famEvent.code == FAMChanged)
						{
							updatedIndex = m_pHandler->fileChanged(fileName);
						}
						else if (famEvent.code == FAMDeleted)
						{
							updatedIndex = m_pHandler->fileDeleted(fileName);
						}
					}
				}

				// Anything else pending ?
				pendingEvents = FAMPending(&famConn);
			}
		}
		else
		{
			if (resumeMonitor == true)
			{
				// Resume
#ifdef DEBUG
				cout << "MonitorThread::doWork: resuming monitoring" << endl;
#endif
				FAMResumeMonitor(&famConn, &famReq);
				resumeMonitor = false;
			}

			// Chances are the timeout expired
			// See if the locations to monitor have changed
			setLocationsToMonitor = true;
		}
	}

	// Stop monitoring and close the connection
	FAMCancelMonitor(&famConn, &famReq);
	FAMClose(&famConn);
}

DirectoryScannerThread::DirectoryScannerThread(const string &dirName,
	unsigned int maxLevel, bool followSymLinks, Mutex *pMutex, Cond *pCondVar) :
	WorkerThread(),
	m_dirName(dirName),
	m_maxLevel(maxLevel),
	m_followSymLinks(followSymLinks),
	m_pMutex(pMutex),
	m_pCondVar(pCondVar),
	m_currentLevel(0)
{
#ifdef DEBUG
	if (m_followSymLinks == true)
	{
		cout << "DirectoryScannerThread: following symlinks" << endl;
	}
#endif
}

DirectoryScannerThread::~DirectoryScannerThread()
{
}

string DirectoryScannerThread::getType(void) const
{
	return "DirectoryScannerThread";
}

bool DirectoryScannerThread::stop(void)
{
	m_done = true;
	m_status = _("Stopped scanning");
	m_status += " ";
	m_status += m_dirName;

	return true;
}

Signal1<bool, const string&>& DirectoryScannerThread::getFileFoundSignal(void)
{
	return m_signalFileFound;
}

void DirectoryScannerThread::foundFile(const string &fileName)
{
	if (fileName.empty() == true)
	{
		return;
	}

	if ((m_pMutex != NULL) &&
		(m_pCondVar != NULL))
	{
		string url(string("file://") + fileName);

		m_pMutex->lock();
		if (m_signalFileFound(url) == true)
		{
			// Another file is needed right now
			m_pMutex->unlock();
		}
		else
		{
#ifdef DEBUG
			cout << "DirectoryScannerThread::foundFile: waiting" << endl;
#endif
			// Don't resume until signaled
			m_pCondVar->wait(*m_pMutex);
			m_pMutex->unlock();
#ifdef DEBUG
			cout << "DirectoryScannerThread::foundFile: signaled" << endl;
#endif
		}
	}
}

bool DirectoryScannerThread::scanDirectory(const string &dirName)
{
	struct stat fileStat;
	int statSuccess = 0;

	if (dirName.empty() == true)
	{
		return false;
	}

	if (m_followSymLinks == false)
	{
		statSuccess = lstat(dirName.c_str(), &fileStat);
	}
	else
	{
		// Stat the files pointed to by symlinks
		statSuccess = stat(dirName.c_str(), &fileStat);
	}

	if (statSuccess == -1)
	{
		return false;
	}

	// Is it a file or a directory ?
	if (S_ISLNK(fileStat.st_mode))
	{
		// This won't happen when m_followSymLinks is true
		return false;
	}
	else if (S_ISREG(fileStat.st_mode))
	{
		// It's actually a file
		foundFile(dirName);
	}
	else if (S_ISDIR(fileStat.st_mode))
	{
		// A directory : scan it
		DIR *pDir = opendir(dirName.c_str());
		if (pDir == NULL)
		{
			return false;
		}
#ifdef DEBUG
		cout << "DirectoryScannerThread::scanDirectory: entering " << dirName << endl;
#endif

		// Iterate through this directory's entries
		struct dirent *pDirEntry = readdir(pDir);
		while ((m_done == false) &&
			(pDirEntry != NULL))
		{
			char *pEntryName = pDirEntry->d_name;
			if (pEntryName != NULL)
			{
				string entryName = dirName;
				if (dirName[dirName.length() - 1] != '/')
				{
					entryName += "/";
				}
				entryName += pEntryName;

				// Skip . .. and dotfiles
				Url urlObj("file://" + entryName);
				if ((urlObj.getFile() == ".") ||
					(urlObj.getFile() == "..") ||
					(urlObj.getFile()[0] == '.') ||
					(lstat(entryName.c_str(), &fileStat) == -1))
				{
					// Next entry
					pDirEntry = readdir(pDir);
					continue;
				}

#ifdef DEBUG
				cout << "DirectoryScannerThread::scanDirectory: stat'ing " << entryName << endl;
#endif
				// File or directory
				if (S_ISREG(fileStat.st_mode))
				{
					foundFile(entryName);
				}
				else if (S_ISDIR(fileStat.st_mode))
				{
					// Can we scan this directory ?
					if ((m_maxLevel == 0) ||
						(m_currentLevel + 1 < m_maxLevel))
					{
						++m_currentLevel;
						scanDirectory(entryName);
#ifdef DEBUG
						cout << "DirectoryScannerThread::scanDirectory: done at level "
							<< m_currentLevel << endl;
#endif
						--m_currentLevel;
					}
				}
			}

			// Next entry
			pDirEntry = readdir(pDir);
		}
		closedir(pDir);
	}

	return true;
}

void DirectoryScannerThread::doWork(void)
{
	if (scanDirectory(m_dirName) == false)
	{
		m_status = _("Couldn't open directory");
		m_status += " ";
		m_status += m_dirName;
	}
}
