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
#include <sigc++/slot.h>
#include <glibmm/dispatcher.h>
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

		void setId(unsigned int id);

		unsigned int getId(void);

		void inBackground(void);

		bool isBackground(void);

		bool operator<(const WorkerThread &other) const;

		virtual bool start(void) = 0;

		virtual std::string getType(void) const = 0;

		virtual bool stop(void) = 0;

		/// Only one thread (the GUI thread) should connect to this, before calling start().
		static Glib::Dispatcher& getFinishedSignal();

		bool isDone(void) const;

		void reset(void);

		std::string getStatus(void) const;

	protected:
		/// Use a Dispatcher, not a Signal, for thread safety
		static Glib::Dispatcher m_signalFinished;
		unsigned int m_id;
		bool m_background;
		bool m_done;
		std::string m_status;

		void emitSignal(void);

	private:
		WorkerThread(const WorkerThread &other);
		WorkerThread &operator=(const WorkerThread &other);

};

class IndexBrowserThread : public WorkerThread
{
	public:
		IndexBrowserThread(const std::string &indexName,
			unsigned int maxDocsCount = 0, unsigned int startDoc = 0);
		~IndexBrowserThread();

		virtual bool start(void);

		std::string getType(void) const;

		std::string getIndexName(void) const;

		unsigned int getDocumentsCount(void) const;

		virtual bool stop(void);

		SigC::Signal3<void, IndexedDocument, unsigned int, std::string>& getUpdateSignal(void);

	protected:
		std::string m_indexName;
		unsigned int m_indexDocsCount;
		unsigned int m_maxDocsCount;
		unsigned int m_startDoc;
		SigC::Signal3<void, IndexedDocument, unsigned int, std::string> m_signalUpdate;

		void do_browsing();

	private:
		IndexBrowserThread(const IndexBrowserThread &other);
		IndexBrowserThread &operator=(const IndexBrowserThread &other);

};

class QueryingThread : public WorkerThread
{
	public:
		QueryingThread(const std::string &engineName, const std::string &engineDisplayableName,
			const std::string &engineOption, const QueryProperties &queryProps);
		virtual ~QueryingThread();

		virtual bool start(void);

		virtual std::string getType(void) const;

		std::string getEngineName(void) const;

		QueryProperties getQuery(void) const;

		const std::vector<Result> &getResults(void) const;

		virtual bool stop(void);

	protected:
		std::string m_engineName;
		std::string m_engineDisplayableName;
		std::string m_engineOption;
		QueryProperties m_queryProps;
		std::vector<Result> m_resultsList;

		void do_querying();

	private:
		QueryingThread(const QueryingThread &other);
		QueryingThread &operator=(const QueryingThread &other);

};

class LabelQueryThread : public WorkerThread
{
	public:
		LabelQueryThread(const std::string &indexName, const std::string &labelName);
		virtual ~LabelQueryThread();

		virtual bool start(void);

		virtual std::string getType(void) const;

		std::string getIndexName(void) const;

		std::string getLabelName(void) const;

		virtual bool stop(void);

		const std::set<unsigned int> &getDocumentsList(void) const;

	protected:
		std::string m_indexName;
		std::string m_labelName;
		std::set<unsigned int> m_documentsList;

		void do_querying();

	private:
		LabelQueryThread(const LabelQueryThread &other);
		LabelQueryThread &operator=(const LabelQueryThread &other);

};

class LabelUpdateThread : public WorkerThread
{
	public:
		LabelUpdateThread(const std::set<std::string> &labelsToDelete,
			const std::map<std::string, std::string> &labelsToRename);
		virtual ~LabelUpdateThread();

		virtual bool start(void);

		virtual std::string getType(void) const;

		virtual bool stop(void);

	protected:
		std::set<std::string> m_labelsToDelete;
		std::map<std::string, std::string> m_labelsToRename;

		void do_update();

	private:
		LabelUpdateThread(const LabelUpdateThread &other);
		LabelUpdateThread &operator=(const LabelUpdateThread &other);

};

class DownloadingThread : public WorkerThread
{
	public:
		DownloadingThread(const std::string url, bool fromCache);
		virtual ~DownloadingThread();

		virtual bool start(void);

		virtual std::string getType(void) const;

		std::string getURL(void) const;

		const Document *getDocument(void) const;

		virtual bool stop(void);

	protected:
		std::string m_url;
		bool m_fromCache;
		Document *m_pDoc;
		bool m_signalAfterDownload;
		DownloaderInterface *m_downloader;

		void do_downloading();

	private:
		DownloadingThread(const DownloadingThread &other);
		DownloadingThread &operator=(const DownloadingThread &other);

};

class IndexingThread : public DownloadingThread
{
	public:
		/// Index a document.
		IndexingThread(const DocumentInfo &docInfo, const std::string &labelName);
		/// Update a document.
		IndexingThread(const DocumentInfo &docInfo, unsigned int docId);
		virtual ~IndexingThread();

		virtual bool start(void);

		virtual std::string getType(void) const;

		const DocumentInfo &getDocumentInfo(void) const;

		std::string getLabelName(void) const;

		const std::set<unsigned int> &getDocumentIDs(void) const;

		bool isNewDocument(void) const;

		virtual bool stop(void);

	protected:
		DocumentInfo m_docInfo;
		std::string m_indexLocation;
		bool m_ignoreRobotsDirectives;
		std::string m_labelName;
		std::set<unsigned int> m_docIdList;
		bool m_update;

		void do_indexing();

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

		virtual bool start(void);

		virtual std::string getType(void) const;

		unsigned int getDocumentsCount(void) const;

		virtual bool stop(void);

	protected:
		std::set<unsigned int> m_docIdList;
		std::set<std::string> m_labelNames;
		std::string m_indexLocation;
		unsigned int m_docsCount;

		void do_unindexing();

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

		virtual bool start(void);

		virtual std::string getType(void) const;

		unsigned int getDocumentID(void) const;

		const DocumentInfo &getDocumentInfo(void) const;

		virtual bool stop(void);

	protected:
		std::string m_indexName;
		unsigned int m_docId;
		DocumentInfo m_docInfo;

		void do_update();

	private:
		UpdateDocumentThread(const UpdateDocumentThread &other);
		UpdateDocumentThread &operator=(const UpdateDocumentThread &other);

};

class ListenerThread : public WorkerThread
{
	public:
		ListenerThread(const std::string &fifoFileName);
		virtual ~ListenerThread();

		virtual bool start(void);

		virtual std::string getType(void) const;

		virtual bool stop(void);

		SigC::Signal2<void, DocumentInfo, std::string>& getReceptionSignal(void);

	protected:
		std::string m_fifoFileName;
		SigC::Signal2<void, DocumentInfo, std::string> m_signalReception;

		void do_listening();

	private:
		ListenerThread(const ListenerThread &other);
		ListenerThread &operator=(const ListenerThread &other);

};

class MonitorThread : public WorkerThread
{
	public:
		MonitorThread(MonitorHandler *pHandler);
		virtual ~MonitorThread();

		virtual bool start(void);

		virtual std::string getType(void) const;

		virtual bool stop(void);

	protected:
		MonitorHandler *m_pHandler;
		long m_numCPUs;

		void do_monitoring();

	private:
		MonitorThread(const MonitorThread &other);
		MonitorThread &operator=(const MonitorThread &other);

};

#endif // _WORKERTHREADS_HH
