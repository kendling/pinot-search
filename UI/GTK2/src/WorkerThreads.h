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

#ifndef _WORKERTHREADS_HH
#define _WORKERTHREADS_HH

#include <time.h>
#include <string>
#include <vector>
#include <queue>
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
#include "DownloaderInterface.h"
#include "QueryProperties.h"

class WorkerThread
{
	public:
		WorkerThread();
		virtual ~WorkerThread();

		static Glib::Dispatcher &getDispatcher(void);

		static void immediateFlush(bool doFlush);

		void setId(unsigned int id);

		unsigned int getId(void);

		void inBackground(void);

		bool isBackground(void);

		bool operator<(const WorkerThread &other) const;

		Glib::Thread *start(void);

		virtual std::string getType(void) const = 0;

		virtual bool stop(void) = 0;

		bool isDone(void) const;

		std::string getStatus(void) const;

	protected:
		/// Use a Dispatcher for thread safety
		static Glib::Dispatcher m_dispatcher;
		static pthread_mutex_t m_dispatcherMutex;
		static bool m_immediateFlush;
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

class ThreadsManager : virtual public SigC::Object
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

		Glib::ustring queue_index(const DocumentInfo &docInfo);

		bool pop_queue(const std::string &urlWasIndexed = "");

		void get_statistics(unsigned int &queueSize);

	protected:
		SigC::Connection m_threadsEndConnection;
		pthread_rwlock_t m_threadsLock;
		pthread_rwlock_t m_listsLock;
		std::map<WorkerThread *, Glib::Thread *> m_threads;
		std::string m_defaultIndexLocation;
		unsigned int m_maxIndexThreads;
		unsigned int m_nextThreadId;
		unsigned int m_backgroundThreadsCount;
		long m_numCPUs;
		SigC::Signal1<void, WorkerThread *> m_onThreadEndSignal;
		std::set<std::string> m_beingIndexed;

		bool read_lock_threads(void);

		bool write_lock_threads(void);

		void unlock_threads(void);

		WorkerThread *get_thread(void);

		Glib::ustring index_document(const DocumentInfo &docInfo);

		void clear_queues(void);

	private:
		ThreadsManager(const ThreadsManager &other);
		ThreadsManager &operator=(const ThreadsManager &other);

};

class ListerThread : public WorkerThread
{
	public:
		ListerThread(const std::string &indexName, unsigned int startDoc);
		~ListerThread();

		std::string getType(void) const;

		std::string getIndexName(void) const;

		unsigned int getStartDoc(void) const;

		const std::vector<DocumentInfo> &getDocuments(void) const;

		unsigned int getDocumentsCount(void) const;

	protected:
		std::string m_indexName;
		unsigned int m_startDoc;
		std::vector<DocumentInfo> m_documentsList;
		unsigned int m_documentsCount;

	private:
		ListerThread(const ListerThread &other);
		ListerThread &operator=(const ListerThread &other);

};

class IndexBrowserThread : public ListerThread
{
	public:
		IndexBrowserThread(const std::string &indexName, unsigned int maxDocsCount,
			unsigned int startDoc = 0);
		~IndexBrowserThread();

		std::string getLabelName(void) const;

		virtual bool stop(void);

	protected:
		unsigned int m_maxDocsCount;

		virtual void doWork(void);

	private:
		IndexBrowserThread(const IndexBrowserThread &other);
		IndexBrowserThread &operator=(const IndexBrowserThread &other);

};

class QueryingThread : public ListerThread
{
	public:
		QueryingThread(const std::string &engineName, const std::string &engineDisplayableName,
			const std::string &engineOption, const QueryProperties &queryProps,
			unsigned int startDoc = 0, bool listingIndex = false);
		virtual ~QueryingThread();

		virtual std::string getType(void) const;

		std::string getEngineName(void) const;

		QueryProperties getQuery(bool &wasCorrected) const;

		std::string getCharset(void) const;

		virtual bool stop(void);

	protected:
		std::string m_engineName;
		std::string m_engineDisplayableName;
		std::string m_engineOption;
		QueryProperties m_queryProps;
		std::string m_resultsCharset;
		bool m_listingIndex;
		bool m_correctedSpelling;

		virtual void processResults(const std::vector<DocumentInfo> &resultsList);

		virtual void processResults(const std::vector<DocumentInfo> &resultsList,
			unsigned int indexId);

		virtual void doWork(void);

	private:
		QueryingThread(const QueryingThread &other);
		QueryingThread &operator=(const QueryingThread &other);

};

class ExpandQueryThread : public WorkerThread
{
	public:
		ExpandQueryThread(const QueryProperties &queryProps,
			const std::set<std::string> &relevantDocs);
		virtual ~ExpandQueryThread();

		virtual std::string getType(void) const;

		QueryProperties getQuery(void) const;

		const std::set<std::string> &getExpandTerms(void) const;

		virtual bool stop(void);

	protected:
		QueryProperties m_queryProps;
		std::set<std::string> m_relevantDocs;
		std::set<std::string> m_expandTerms;

		virtual void doWork(void);

	private:
		ExpandQueryThread(const ExpandQueryThread &other);
		ExpandQueryThread &operator=(const ExpandQueryThread &other);

};

class LabelUpdateThread : public WorkerThread
{
	public:
		LabelUpdateThread(const std::set<std::string> &labelsToDelete,
			const std::map<std::string, std::string> &labelsToRename);
		LabelUpdateThread(const std::string &labelName,
			const std::set<unsigned int> &docsIds,
			const std::set<unsigned int> &daemonIds);

		virtual ~LabelUpdateThread();

		virtual std::string getType(void) const;

		virtual bool stop(void);

	protected:
		std::set<std::string> m_labelsToDelete;
		std::map<std::string, std::string> m_labelsToRename;
		std::string m_labelName;
		std::set<unsigned int> m_docsIds;
		std::set<unsigned int> m_daemonIds;

		virtual void doWork(void);

	private:
		LabelUpdateThread(const LabelUpdateThread &other);
		LabelUpdateThread &operator=(const LabelUpdateThread &other);

};

class DownloadingThread : public WorkerThread
{
	public:
		DownloadingThread(const DocumentInfo &docInfo);
		virtual ~DownloadingThread();

		virtual std::string getType(void) const;

		std::string getURL(void) const;

		const Document *getDocument(void) const;

		virtual bool stop(void);

	protected:
		DocumentInfo m_docInfo;
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
		IndexingThread(const DocumentInfo &docInfo, const std::string &indexLocation,
			bool allowAllMIMETypes = true);
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

class StartDaemonThread : public WorkerThread
{
	public:
		// Start the daemon
		StartDaemonThread();
		virtual ~StartDaemonThread();

		virtual std::string getType(void) const;

		virtual bool stop(void);

	protected:
		virtual void doWork(void);

	private:
		StartDaemonThread(const StartDaemonThread &other);
		StartDaemonThread &operator=(const StartDaemonThread &other);

};

#endif // _WORKERTHREADS_HH
