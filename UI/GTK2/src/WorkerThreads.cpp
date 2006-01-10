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
#include <iostream>
#include <fstream>
#include <sigc++/class_slot.h>
#include <glibmm/thread.h>

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

// A function object to delete pointers from a set with for_each()
struct DeleteSetPointer
{
	template<typename T>
	void operator()(const T *ptr) const
	{
		delete ptr;
	}
};

Dispatcher WorkerThread::m_dispatcher;

Dispatcher &WorkerThread::getDispatcher(void)
{
	return m_dispatcher;
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
	if (m_threads.empty() == false)
	{
		for_each(m_threads.begin(), m_threads.end(), DeleteSetPointer());
	}
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
	WorkerThread *pThread = NULL;

	// Get the first thread that's finished
	if (read_lock(1) == true)
	{
		for (set<WorkerThread *>::iterator threadIter = m_threads.begin();
			threadIter != m_threads.end(); ++threadIter)
		{
			if ((*threadIter)->isDone() == true)
			{
				// This one will do...
				pThread = (*threadIter);
				// Remove it
				m_threads.erase(threadIter);
				break;
			}
#ifdef DEBUG
			cout << "ThreadsManager::on_thread_end: thread "
				<< (*threadIter)->getId() << " is not done" << endl;
#endif
		}

		unlock();
	}

	if (pThread == NULL)
	{
		return NULL;
	}

	if (pThread->isBackground() == true)
	{
		--m_backgroundThreadsCount;
	}

	return pThread;
}

bool ThreadsManager::start_thread(WorkerThread *pNewThread, bool inBackground)
{
	bool insertedThread = false;

	if (pNewThread == NULL)
	{
		return false;
	}

	pNewThread->setId(m_nextId);
	if (write_lock(2) == true)
	{
		pair<set<WorkerThread *>::iterator, bool> insertPair = m_threads.insert(pNewThread);
		insertedThread = insertPair.second;

		unlock();
	}

	// Was it inserted ?
	if (insertedThread == false)
	{
		// No, it wasn't
#ifdef DEBUG
		cout << "ThreadsManager::start_thread: couldn't start "
			<< pNewThread->getType() << endl;
#endif
		return false;
	}

	// Start the thread
	if (inBackground == true)
	{
		pNewThread->inBackground();
		++m_backgroundThreadsCount;
	}
	pNewThread->start();
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
	if (read_lock(4) == true)
	{
		for (set<WorkerThread *>::iterator threadIter = m_threads.begin();
			threadIter != m_threads.end(); ++threadIter)
		{
			// Stop all threads
			// FIXME: what if one thread doesn't stop ?
			(*threadIter)->stop();
		}

		unlock();
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
	unsigned int maxDocsCount, unsigned int startDoc) :
	WorkerThread()
{
	m_indexName = indexName;
	m_indexDocsCount = 0;
	m_maxDocsCount = maxDocsCount;
	m_startDoc = startDoc;
}

IndexBrowserThread::~IndexBrowserThread()
{
}

bool IndexBrowserThread::start(void)
{
	// Create a non-joinable thread
	Thread::create(slot_class(*this, &IndexBrowserThread::do_browsing), false);

	return true;
}

string IndexBrowserThread::getType(void) const
{
	return "IndexBrowserThread";
}

string IndexBrowserThread::getIndexName(void) const
{
	return m_indexName;
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

void IndexBrowserThread::do_browsing()
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
		emitSignal();
		return;
	}

	// Get the index at that location
	XapianIndex index(mapIter->second);
	if (index.isGood() == false)
	{
		m_status = _("Index error on");
		m_status += " ";
		m_status += mapIter->second;
		emitSignal();
		return;
	}

	m_indexDocsCount = index.getDocumentsCount();
	if (m_indexDocsCount == 0)
	{
#ifdef DEBUG
		cout << "IndexBrowserThread::do_browsing: no documents" << endl;
#endif
		emitSignal();
		return;
	}

#ifdef DEBUG
	cout << "IndexBrowserThread::do_browsing: " << m_maxDocsCount << " off " << m_indexDocsCount
		<< " documents to browse, starting at " << m_startDoc << endl;
#endif
	index.listDocuments(docIDList, m_maxDocsCount, m_startDoc);
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
		else cout << "IndexBrowserThread::do_browsing: couldn't retrieve document " << docId << endl;
#endif
	}

	emitSignal();
}

QueryingThread::QueryingThread(const string &engineName, const string &engineDisplayableName,
	const string &engineOption, const QueryProperties &queryProps) :
	m_queryProps(queryProps), WorkerThread()
{
	m_engineName = engineName;
	m_engineDisplayableName = engineDisplayableName;
	m_engineOption = engineOption;
}

QueryingThread::~QueryingThread()
{
}

bool QueryingThread::start(void)
{
	// Create a non-joinable thread
	Thread::create(slot_class(*this, &QueryingThread::do_querying), false);

	return true;
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

void QueryingThread::do_querying()
{
	// Get the SearchEngine
	SearchEngineInterface *engine = SearchEngineFactory::getSearchEngine(m_engineName, m_engineOption);
	if (engine == NULL)
	{
		m_status = _("Couldn't create search engine");
		m_status += " ";
		m_status += m_engineDisplayableName;
		emitSignal();
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

	emitSignal();
}

LabelQueryThread::LabelQueryThread(const string &indexName, const string &labelName) :
	WorkerThread()
{
	m_indexName = indexName;
	m_labelName = labelName;
}

LabelQueryThread::~LabelQueryThread()
{
}

bool LabelQueryThread::start(void)
{
	// Create a non-joinable thread
	Thread::create(slot_class(*this, &LabelQueryThread::do_querying), false);

	return true;
}

string LabelQueryThread::getType(void) const
{
	return "LabelQueryThread";
}

string LabelQueryThread::getIndexName(void) const
{
	return m_indexName;
}

string LabelQueryThread::getLabelName(void) const
{
	return m_labelName;
}

bool LabelQueryThread::stop(void)
{
	m_done = true;
	m_status = _("Stopped querying index labels");
	return true;
}

const set<unsigned int> &LabelQueryThread::getDocumentsList(void) const
{
	return m_documentsList;
}

void LabelQueryThread::do_querying()
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
		emitSignal();
		return;
	}

	XapianIndex index(mapIter->second);
	if (index.isGood() == false)
	{
		m_status = _("Index error on");
		m_status += " ";
		m_status += mapIter->second;
		emitSignal();
		return;
	}

	index.getDocumentsWithLabel(m_labelName, m_documentsList);

	emitSignal();
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

bool LabelUpdateThread::start(void)
{
	// Create a non-joinable thread
	Thread::create(slot_class(*this, &LabelUpdateThread::do_update), false);

	return true;
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

void LabelUpdateThread::do_update()
{
	XapianIndex docsIndex(PinotSettings::getInstance().m_indexLocation);
	if (docsIndex.isGood() == false)
	{
		m_status = _("Index error on");
		m_status += " ";
		m_status += PinotSettings::getInstance().m_indexLocation;
		emitSignal();
		return;
	}

	XapianIndex mailIndex(PinotSettings::getInstance().m_mailIndexLocation);
	if (mailIndex.isGood() == false)
	{
		m_status = _("Index error on");
		m_status += " ";
		m_status += PinotSettings::getInstance().m_mailIndexLocation;
		emitSignal();
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

	emitSignal();
}

DownloadingThread::DownloadingThread(const string url, bool fromCache) :
	WorkerThread()
{
	m_url = url;
	m_fromCache = fromCache;
	m_pDoc = NULL;
	// This is for sub-classes that need to things after the document has been downloaded
	m_signalAfterDownload = true;
	m_downloader = NULL;
}

DownloadingThread::~DownloadingThread()
{
	if (m_pDoc != NULL)
	{
		delete m_pDoc;
	}
	if (m_downloader != NULL)
	{
		delete m_downloader;
	}
}

bool DownloadingThread::start(void)
{
	// Create a non-joinable thread
	Thread::create(slot_class(*this, &DownloadingThread::do_downloading), false);

	return true;
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
	if (m_downloader->stop() == true)
	{
		m_done = true;
		m_status = _("Stopped retrieval of");
		m_status += " ";
		m_status += m_url;
		return true;
	}

	return false;
}

void DownloadingThread::do_downloading()
{
	if (m_downloader != NULL)
	{
		delete m_downloader;
		m_downloader = NULL;
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
		cout << "DownloadingThread::do_downloading: got cached page" << endl;
#endif
	}
	else
	{
		// Get a Downloader, the default one will do
		m_downloader = DownloaderFactory::getDownloader(thisUrl.getProtocol(), "");
		if (m_downloader == NULL)
		{
			m_status = _("Couldn't obtain downloader for protocol");
			m_status += " ";
			m_status += thisUrl.getProtocol();
		}
		else if (m_done == false)
		{
			DocumentInfo docInfo("", m_url, "", "");

			m_pDoc = m_downloader->retrieveUrl(docInfo);
		}
	}

	if (m_pDoc == NULL)
	{
		m_status = _("Couldn't retrieve");
		m_status += " ";
		m_status += m_url;
	}

	// Signal ?
	if (m_signalAfterDownload == true)
	{
		emitSignal();
	}
}

IndexingThread::IndexingThread(const DocumentInfo &docInfo, const string &labelName) :
	DownloadingThread(docInfo.getLocation(), false)
{
	m_docInfo = docInfo;
	m_indexLocation = PinotSettings::getInstance().m_indexLocation;
	m_ignoreRobotsDirectives = PinotSettings::getInstance().m_ignoreRobotsDirectives;
	m_labelName = labelName;
	// This is not an update
	m_update = false;
	// Don't trigger signal after the document has been downloaded
	m_signalAfterDownload = false;
}

IndexingThread::IndexingThread(const DocumentInfo &docInfo, unsigned int docId) :
	DownloadingThread(docInfo.getLocation(), false)
{
	m_docInfo = docInfo;
	m_indexLocation = PinotSettings::getInstance().m_indexLocation;
	// Ignore robots directives on updates
	m_ignoreRobotsDirectives = true;
	m_docId = docId;
	m_update = true;
	// Don't trigger signal after the document has been downloaded
	m_signalAfterDownload = false;
}

IndexingThread::~IndexingThread()
{
}

bool IndexingThread::start(void)
{
	// Create a non-joinable thread
	Thread::create(slot_class(*this, &IndexingThread::do_indexing), false);

	return true;
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

void IndexingThread::do_indexing()
{
	// First things first, get the index
	XapianIndex index(m_indexLocation);
	if (index.isGood() == false)
	{
		m_status = _("Index error on");
		m_status += " ";
		m_status += m_indexLocation;
		emitSignal();
		return;
	}

	do_downloading();
#ifdef DEBUG
	cout << "IndexingThread::do_indexing: downloaded !" << endl;
#endif

	if (m_pDoc == NULL)
	{
		m_status = _("Couldn't retrieve");
		m_status += " ";
		m_status += m_url;
	}
	else
	{
		Url urlObj(m_url);
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
			emitSignal();
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
		cout << "IndexingThread::do_indexing: title is " << m_pDoc->getTitle() << endl;
#endif

		// Tokenize this document
		Tokenizer *pTokens = TokenizerFactory::getTokenizerByType(m_docInfo.getType(), m_pDoc);
		if (pTokens == NULL)
		{
			m_status = _("Couln't tokenize");
			m_status += " ";
			m_status += m_url;
			emitSignal();
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
				emitSignal();
				return;
			}
		}

		if (m_done == false)
		{
			index.setStemmingMode(IndexInterface::STORE_BOTH);

			// Update an existing document or add to the index ?
			if (m_update == true)
			{
				success = index.updateDocument(m_docId, *pTokens);
#ifdef DEBUG
				cout << "IndexingThread::do_indexing: updated " << m_docId << endl;
#endif
			}
			else
			{
				set<string> labels;
				unsigned int docId = 0;

				// Save the new document ID
				labels.insert(m_labelName);
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
			else
			{
				// Flush the index
				index.flush();

				// The document properties may have changed
				index.getDocumentInfo(m_docId, m_docInfo);
			}
		}

		delete pTokens;
	}

	emitSignal();
}

UnindexingThread::UnindexingThread(const set<unsigned int> &docIdList) :
	WorkerThread(),
	m_docsCount(0)
{
	copy(docIdList.begin(), docIdList.end(), inserter(m_docIdList, m_docIdList.begin()));
	m_indexLocation = PinotSettings::getInstance().m_indexLocation;
}

UnindexingThread::UnindexingThread(const set<string> &labelNames, const string &indexLocation) :
	WorkerThread(),
	m_docsCount(0)
{
	copy(labelNames.begin(), labelNames.end(), inserter(m_labelNames, m_labelNames.begin()));
	if (indexLocation.empty() == true)
	{
		m_indexLocation = PinotSettings::getInstance().m_indexLocation;
	}
	else
	{
		m_indexLocation = indexLocation;
	}
}

UnindexingThread::~UnindexingThread()
{
}

bool UnindexingThread::start(void)
{
	// Create a non-joinable thread
	Thread::create(slot_class(*this, &UnindexingThread::do_unindexing), false);

	return true;
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

void UnindexingThread::do_unindexing()
{
	if (m_done == false)
	{
		XapianIndex index(m_indexLocation);

		if (index.isGood() == false)
		{
			m_status = _("Index error on");
			m_status += " ";
			m_status += m_indexLocation;
			emitSignal();
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
				else cout << "UnindexingThread::do_unindexing: couldn't remove " << docId << endl;
#endif
			}
#ifdef DEBUG
			cout << "UnindexingThread::do_unindexing: removed " << m_docsCount << " documents" << endl;
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

	emitSignal();
}

UpdateDocumentThread::UpdateDocumentThread(const string &indexName,
	unsigned int docId, const DocumentInfo &docInfo) :
	WorkerThread()
{
	m_indexName = indexName;
	m_docId = docId;
	m_docInfo = docInfo;
}

UpdateDocumentThread::~UpdateDocumentThread()
{
}

bool UpdateDocumentThread::start(void)
{
	// Create a non-joinable thread
	Thread::create(slot_class(*this, &UpdateDocumentThread::do_update), false);

	return true;
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
	m_status = _("Stopped document update for ");
	m_status += " ";
	m_status += m_docId;

	return true;
}

void UpdateDocumentThread::do_update()
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
			emitSignal();
			return;
		}

		// Get the index at that location
		XapianIndex index(mapIter->second);
		if (index.isGood() == false)
		{
			m_status = _("Index error on");
			m_status += " ";
			m_status += mapIter->second;
			emitSignal();
			return;
		}

		if (index.updateDocumentInfo(m_docId, m_docInfo) == false)
		{
			m_status = _("Couldn't update document");
		}
		else
		{
			// OK
			m_status = "";
			// Flush the index
			index.flush();
		}
	}

	emitSignal();
}

ListenerThread::ListenerThread(const string &fifoFileName) :
	WorkerThread()
{
	m_fifoFileName = fifoFileName;
}

ListenerThread::~ListenerThread()
{
}

bool ListenerThread::start(void)
{
	// Create a non-joinable thread
	Thread::create(slot_class(*this, &ListenerThread::do_listening), false);

	return true;
}

string ListenerThread::getType(void) const
{
	return "ListenerThread";
}

bool ListenerThread::stop(void)
{
	m_done = true;
	m_status = _("Stopped listening on");
	m_status += " ";
	m_status += m_fifoFileName;

	return true;
}

Signal2<void, DocumentInfo, string>& ListenerThread::getReceptionSignal(void)
{
	return m_signalReception;
}

void ListenerThread::do_listening()
{
	if (unlink(m_fifoFileName.c_str()) != 0)
	{
#ifdef DEBUG
		cout << "ListenerThread::do_listening: couldn't delete FIFO at " << m_fifoFileName << endl;
#endif
	}

	if (mkfifo(m_fifoFileName.c_str(), S_IRUSR|S_IWUSR) != 0)
	{
#ifdef DEBUG
		cout << "ListenerThread::do_listening: couldn't create FIFO at " << m_fifoFileName << endl;
#endif
	}

	// Ignore SIGPIPE
	if (sigset(SIGPIPE, SIG_IGN) == SIG_ERR)
	{
#ifdef DEBUG
		cout << "ListenerThread::do_listening: couldn't ignore SIGPIPE" << endl;
#endif
	}

	// Open the FIFO
	int fd = open(m_fifoFileName.c_str(), O_RDWR);
	if (fd != -1)
	{
		// Set the file in non-blocking mode
		int flags = fcntl(fd, F_GETFL);
		flags |= O_NONBLOCK;
		fcntl(fd, F_SETFL, (long)flags);

		fd_set listenSet;
		FD_ZERO(&listenSet);
		FD_SET(fd, &listenSet);

		// Listen and wait for something to read
		while (m_done == false)
		{
			int fdCount = select(fd + 1, &listenSet, NULL, NULL, NULL);
			if ((fdCount < 0) &&
				(errno != EINTR))
			{
#ifdef DEBUG
				cout << "ListenerThread::do_listening: select() failed" << endl;
#endif
				break;
			}
			else if (FD_ISSET(fd, &listenSet))
			{
				string xmlMsg;
				char buffer[1024];

#ifdef DEBUG
				cout << "ListenerThread::do_listening: reading..." << endl;
#endif
				ssize_t bytes = read(fd, buffer, 1024);
				while (bytes > 0)
				{
					xmlMsg += string(buffer, bytes);
#ifdef DEBUG
					cout << "ListenerThread::do_listening: read " << bytes << " bytes" << endl;
#endif
					bytes = read(fd, buffer, 1024);
				}

				// FIXME: ensure the XML is valid, use libxml++ parser
				string location = StringManip::extractField(xmlMsg, "<location>", "</location>");
				Url urlObj(location);
				DocumentInfo docInfo(StringManip::extractField(xmlMsg, "<title>", "</title>"),
					location, MIMEScanner::scanUrl(urlObj), "");
				string labelName = StringManip::extractField(xmlMsg, "<label>", "</label>");
				string content = StringManip::extractField(xmlMsg, "<content>", "</content>");
#ifdef DEBUG
				cout << "ListenerThread::do_listening: " << content.length() << " bytes of content" << endl;
#endif

				// Signal
				m_signalReception(docInfo, labelName);
			}
		}

		close(fd);
	}
	else
	{
		m_status = _("Couldn't read FIFO at");
		m_status += " ";
		m_status += m_fifoFileName;
	}

	emitSignal();
}

MonitorThread::MonitorThread(MonitorHandler *pHandler) :
	WorkerThread()
{
	m_pHandler = pHandler;
	m_numCPUs = sysconf(_SC_NPROCESSORS_ONLN);
}

MonitorThread::~MonitorThread()
{
	// It's our responsability to delete the MonitorHandler object
	if (m_pHandler != NULL)
	{
		delete m_pHandler;
	}
}

bool MonitorThread::start(void)
{
	// Create a non-joinable thread
	Thread::create(slot_class(*this, &MonitorThread::do_monitoring), false);

	return true;
}

string MonitorThread::getType(void) const
{
	return "MonitorThread";
}

bool MonitorThread::stop(void)
{
	m_done = true;
	m_status = _("Stopped monitoring");

	return true;
}

void MonitorThread::do_monitoring()
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
		emitSignal();
		return;
	}

	// Wait for something to happen
	while (m_done == false)
	{
#ifdef DEBUG
		cout << "MonitorThread::do_monitoring: checking locations" << endl;
#endif
		if ((setLocationsToMonitor == true) &&
			(m_pHandler->getFileSystemLocations(fsLocations) > 0) &&
			(m_pHandler->hasNewLocations() == true))
		{
			// Tell FAM what we want to monitor
#ifdef DEBUG
			cout << "MonitorThread::do_monitoring: change detected" << endl;
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
				emitSignal();
				return;
			}

			// Go through the locations map
			for (map<unsigned long, string>::const_iterator fsIter = fsLocations.begin(); fsIter != fsLocations.end(); ++fsIter)
			{
				string fsLocation = fsIter->second;
				struct stat fileStat;

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
				cout << "MonitorThread::do_monitoring: added " << fsLocation << ", " << famStatus << endl;
#endif
			}
		}
		setLocationsToMonitor = false;

		int fd = FAMCONNECTION_GETFD(&famConn);

		fd_set listenSet;
		FD_ZERO(&listenSet);
		FD_SET(fd, &listenSet);

		struct timeval selectTimeout;
		selectTimeout.tv_sec = 60;
		selectTimeout.tv_usec = 0;

		int fdCount = select(fd + 1, &listenSet, NULL, NULL, &selectTimeout);
		if ((fdCount < 0) &&
			(errno != EINTR))
		{
#ifdef DEBUG
			cout << "MonitorThread::do_monitoring: select() failed" << endl;
#endif
			break;
		}
		else if (FD_ISSET(fd, &listenSet))
		{
			// There might be more than one event waiting...
			int pendingEvents = FAMPending(&famConn);
			if (pendingEvents < 0)
			{
#ifdef DEBUG
				cout << "MonitorThread::do_monitoring: FAMPending() failed" << endl;
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
						cout << "MonitorThread::do_monitoring: cancelling monitoring because of load (" << averageLoad[0] << ")" << endl;
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
					cout << "MonitorThread::do_monitoring: event " << famEvent.code
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
				cout << "MonitorThread::do_monitoring: resuming monitoring" << endl;
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

	emitSignal();
}

DirectoryScannerThread::DirectoryScannerThread(const std::string &dirName,
			unsigned int maxLevel, pthread_mutex_t *pMutex,
			pthread_cond_t *pCondVar) :
	WorkerThread(),
	m_dirName(dirName),
	m_maxLevel(maxLevel),
	m_pMutex(pMutex),
	m_pCondVar(pCondVar),
	m_currentLevel(0)
{
}

DirectoryScannerThread::~DirectoryScannerThread()
{
}

bool DirectoryScannerThread::start(void)
{
	// Create a non-joinable thread
	Thread::create(slot_class(*this, &DirectoryScannerThread::do_scanning), false);

	return true;
}

string DirectoryScannerThread::getType(void) const
{
	return "DirectoryScannerThread";
}

bool DirectoryScannerThread::stop(void)
{
	m_done = true;
	m_status = _("Stopped scanning ");
	m_status += " ";
	m_status += m_dirName;

	return true;
}

Signal1<bool, const string&>& DirectoryScannerThread::getFileFoundSignal(void)
{
	return m_signalFileFound;
}

void DirectoryScannerThread::found_file(const string &fileName)
{
	if (fileName.empty() == true)
	{
		return;
	}

	if ((m_pMutex != NULL) &&
		(m_pCondVar != NULL) &&
		(pthread_mutex_lock(m_pMutex) == 0))
	{
		if (m_signalFileFound(fileName) == true)
		{
			// Another file is needed right now
			pthread_mutex_unlock(m_pMutex);
		}
		else
		{
#ifdef DEBUG
			cout << "DirectoryScannerThread::found_file: waiting" << endl;
#endif
			// Don't resume until signaled
			pthread_cond_wait(m_pCondVar, m_pMutex);
			pthread_mutex_unlock(m_pMutex);
#ifdef DEBUG
			cout << "DirectoryScannerThread::found_file: signaled" << endl;
#endif
		}
	}
}

bool DirectoryScannerThread::scan_directory(const string &dirName)
{
	struct stat fileStat;

	if ((dirName.empty() == true) ||
		(lstat(dirName.c_str(), &fileStat) == -1))
	{
		return false;
	}

	// Is it a file or a directory ?
	if (S_ISLNK(fileStat.st_mode))
	{
		return false;
	}
	else if (S_ISREG(fileStat.st_mode))
	{
		// It's actually a file
		found_file(dirName);
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
		cout << "DirectoryScannerThread::scan_directory: entering " << dirName << endl;
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
				cout << "DirectoryScannerThread::scan_directory: stat'ing " << entryName << endl;
#endif
				// File or directory
				if (S_ISREG(fileStat.st_mode))
				{
					found_file(entryName);
				}
				else if (S_ISDIR(fileStat.st_mode))
				{
					// Can we scan this directory ?
					if ((m_maxLevel == 0) ||
						(m_currentLevel + 1 < m_maxLevel))
					{
						++m_currentLevel;
						scan_directory(entryName);
#ifdef DEBUG
						cout << "DirectoryScannerThread::scan_directory: done at level "
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

void DirectoryScannerThread::do_scanning()
{
	if (scan_directory(m_dirName) == false)
	{
		m_status = _("Couldn't open directory ");
		m_status += m_dirName;
	}
	emitSignal();
}
