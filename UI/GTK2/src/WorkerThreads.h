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

#ifndef _WORKERTHREADS_HH
#define _WORKERTHREADS_HH

#include <time.h>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <pthread.h>
#include <sigc++/object.h>
#include <sigc++/slot.h>
#include <sigc++/connection.h>
#include <glibmm/dispatcher.h>
#include <glibmm/thread.h>
#include <glibmm/ustring.h>

#include "Document.h"
#include "IndexedDocument.h"
#include "DownloaderInterface.h"
#include "QueryProperties.h"
#include "Result.h"
#include "MonitorHandler.h"

class WorkerThread
{
	public:
		WorkerThread();
		virtual ~WorkerThread();

		static Glib::Dispatcher &getDispatcher(void);

		void setId(unsigned int id);

		unsigned int getId(void);

		void inBackground(void);

		bool isBackground(void);

		bool operator<(const WorkerThread &other) const;

		Glib::Thread *start(void);

		virtual std::string getType(void) const = 0;

		virtual bool stop(void) = 0;

		bool isDone(void) const;

		void reset(void);

		std::string getStatus(void) const;

	protected:
		/// Use a Dispatcher for thread safety
		static Glib::Dispatcher m_dispatcher;
		bool m_joinable;
		unsigned int m_id;
		bool m_background;
		bool m_done;
		std::string m_status;

		void threadHandler(void);

		virtual void doWork(void) = 0;

		void emitSignal(void);

	private:
		WorkerThread(const WorkerThread &other);
		WorkerThread &operator=(const WorkerThread &other);

};

class ThreadsManager : public SigC::Object
{
	public:
		ThreadsManager(const std::string &defaultIndexLocation,
			unsigned int maxIndexThreads);
		virtual ~ThreadsManager();

		bool start_thread(WorkerThread *pWorkerThread, bool inBackground = false);

		unsigned int get_threads_count(void);

		bool has_threads(void);

		void stop_threads(void);

		virtual void connect(void);

		virtual void disconnect(void);

		void on_thread_signal();

		bool read_lock_lists(void);

		bool write_lock_lists(void);

		void unlock_lists(void);

		bool queue_index(const DocumentInfo &docInfo);

		bool pop_queue(void);

		std::set<std::string> m_beingIndexed;

	protected:
		SigC::Connection m_threadsEndConnection;
		pthread_rwlock_t m_threadsLock;
		pthread_rwlock_t m_listsLock;
		std::map<WorkerThread *, Glib::Thread *> m_threads;
		std::string m_defaultIndexLocation;
		unsigned int m_maxIndexThreads;
		unsigned int m_nextId;
		unsigned int m_backgroundThreadsCount;
		SigC::Signal1<void, WorkerThread *> m_onThreadEndSignal;
		std::set<DocumentInfo> m_indexQueue;

		bool read_lock_threads(void);
		bool write_lock_threads(void);
		void unlock_threads(void);
		WorkerThread *get_thread(void);
		bool index_document(const DocumentInfo &docInfo);

	private:
		ThreadsManager(const ThreadsManager &other);
		ThreadsManager &operator=(const ThreadsManager &other);

};

class IndexBrowserThread : public WorkerThread
{
	public:
		IndexBrowserThread(const std::string &indexName, const std::string &labelName,
			unsigned int maxDocsCount, unsigned int startDoc);
		~IndexBrowserThread();

		std::string getType(void) const;

		std::string getIndexName(void) const;

		std::string getLabelName(void) const;

		unsigned int getDocumentsCount(void) const;

		virtual bool stop(void);

		SigC::Signal3<void, IndexedDocument, unsigned int, std::string>& getUpdateSignal(void);

	protected:
		std::string m_indexName;
		std::string m_labelName;
		unsigned int m_indexDocsCount;
		unsigned int m_maxDocsCount;
		unsigned int m_startDoc;
		SigC::Signal3<void, IndexedDocument, unsigned int, std::string> m_signalUpdate;

		virtual void doWork(void);

	private:
		IndexBrowserThread(const IndexBrowserThread &other);
		IndexBrowserThread &operator=(const IndexBrowserThread &other);

};

class QueryingThread : public WorkerThread
{
	public:
		QueryingThread(const std::string &engineName, const std::string &engineDisplayableName,
			const std::string &engineOption, const QueryProperties &queryProps);
		QueryingThread(const std::string &engineName, const std::string &engineDisplayableName,
			const std::string &engineOption, const QueryProperties &queryProps,
			const std::set<unsigned int> &relevantDocs);
		virtual ~QueryingThread();

		virtual std::string getType(void) const;

		std::string getEngineName(void) const;

		QueryProperties getQuery(void) const;

		const std::vector<Result> &getResults(std::string &charset) const;

		const std::set<std::string> &getExpandTerms(void) const;

		virtual bool stop(void);

	protected:
		std::string m_engineName;
		std::string m_engineDisplayableName;
		std::string m_engineOption;
		QueryProperties m_queryProps;
		std::set<unsigned int> m_relevantDocs;
		std::vector<Result> m_resultsList;
		std::string m_resultsCharset;
		std::set<std::string> m_expandTerms;

		virtual void doWork(void);

	private:
		QueryingThread(const QueryingThread &other);
		QueryingThread &operator=(const QueryingThread &other);

};

class LabelUpdateThread : public WorkerThread
{
	public:
		LabelUpdateThread(const std::set<std::string> &labelsToDelete,
			const std::map<std::string, std::string> &labelsToRename);
		virtual ~LabelUpdateThread();

		virtual std::string getType(void) const;

		virtual bool stop(void);

	protected:
		std::set<std::string> m_labelsToDelete;
		std::map<std::string, std::string> m_labelsToRename;

		virtual void doWork(void);

	private:
		LabelUpdateThread(const LabelUpdateThread &other);
		LabelUpdateThread &operator=(const LabelUpdateThread &other);

};

class DownloadingThread : public WorkerThread
{
	public:
		DownloadingThread(const DocumentInfo &docInfo, bool fromCache);
		virtual ~DownloadingThread();

		virtual std::string getType(void) const;

		std::string getURL(void) const;

		const Document *getDocument(void) const;

		virtual bool stop(void);

	protected:
		DocumentInfo m_docInfo;
		bool m_fromCache;
		Document *m_pDoc;
		DownloaderInterface *m_pDownloader;

		virtual void doWork(void);

	private:
		DownloadingThread(const DownloadingThread &other);
		DownloadingThread &operator=(const DownloadingThread &other);

};

class IndexingThread : public DownloadingThread
{
	public:
		IndexingThread(const DocumentInfo &docInfo, unsigned int docId,
			const std::string &indexLocation, bool allowAllMIMETypes = true);
		virtual ~IndexingThread();

		virtual std::string getType(void) const;

		const DocumentInfo &getDocumentInfo(void) const;

		std::string getLabelName(void) const;

		unsigned int getDocumentID(void) const;

		bool isNewDocument(void) const;

		virtual bool stop(void);

	protected:
		DocumentInfo m_docInfo;
		unsigned int m_docId;
		std::string m_indexLocation;
		bool m_allowAllMIMETypes;
		bool m_ignoreRobotsDirectives;
		bool m_update;

		virtual void doWork(void);

	private:
		IndexingThread(const IndexingThread &other);
		IndexingThread &operator=(const IndexingThread &other);

};

class UnindexingThread : public WorkerThread
{
	public:
		// Unindex documents from the internal index
		UnindexingThread(const std::set<unsigned int> &docIdList);
		// Unindex from the given index documents that have one of the labels
		UnindexingThread(const std::set<std::string> &labelNames, const std::string &indexLocation);
		virtual ~UnindexingThread();

		virtual std::string getType(void) const;

		unsigned int getDocumentsCount(void) const;

		virtual bool stop(void);

	protected:
		std::set<unsigned int> m_docIdList;
		std::set<std::string> m_labelNames;
		std::string m_indexLocation;
		unsigned int m_docsCount;

		virtual void doWork(void);

	private:
		UnindexingThread(const UnindexingThread &other);
		UnindexingThread &operator=(const UnindexingThread &other);

};

class UpdateDocumentThread : public WorkerThread
{
	public:
		// Update a document's properties
		UpdateDocumentThread(const std::string &indexName,
			unsigned int docId, const DocumentInfo &docInfo);
		virtual ~UpdateDocumentThread();

		virtual std::string getType(void) const;

		unsigned int getDocumentID(void) const;

		const DocumentInfo &getDocumentInfo(void) const;

		virtual bool stop(void);

	protected:
		std::string m_indexName;
		unsigned int m_docId;
		DocumentInfo m_docInfo;

		virtual void doWork(void);

	private:
		UpdateDocumentThread(const UpdateDocumentThread &other);
		UpdateDocumentThread &operator=(const UpdateDocumentThread &other);

};

class MonitorThread : public WorkerThread
{
	public:
		MonitorThread(MonitorHandler *pHandler);
		virtual ~MonitorThread();

		virtual std::string getType(void) const;

		virtual bool stop(void);

	protected:
		int m_ctrlReadPipe;
		int m_ctrlWritePipe;
		MonitorHandler *m_pHandler;
		long m_numCPUs;

		virtual void doWork(void);

	private:
		MonitorThread(const MonitorThread &other);
		MonitorThread &operator=(const MonitorThread &other);

};

class DirectoryScannerThread : public WorkerThread
{
	public:
		DirectoryScannerThread(const std::string &dirName,
			unsigned int maxLevel, bool followSymLinks,
			Glib::Mutex *pMutex, Glib::Cond *pCondVar);
		virtual ~DirectoryScannerThread();

		virtual std::string getType(void) const;

		virtual std::string getDirectory(void) const;

		virtual bool stop(void);

		SigC::Signal1<bool, const std::string&>& getFileFoundSignal(void);

	protected:
		std::string m_dirName;
		unsigned int m_maxLevel;
		bool m_followSymLinks;
		Glib::Mutex *m_pMutex;
		Glib::Cond *m_pCondVar;
		unsigned int m_currentLevel;
		SigC::Signal1<bool, const std::string&> m_signalFileFound;

		void foundFile(const std::string &fileName);
		bool scanDirectory(const std::string &dirName);
		virtual void doWork(void);

	private:
		DirectoryScannerThread(const DirectoryScannerThread &other);
		DirectoryScannerThread &operator=(const DirectoryScannerThread &other);

};

#endif // _WORKERTHREADS_HH
