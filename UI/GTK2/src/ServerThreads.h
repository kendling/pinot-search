/*
 *  Copyright 2005-2009 Fabrice Colin
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

#ifndef _SERVERTHREADS_HH
#define _SERVERTHREADS_HH

#include <string>
#include <vector>
#include <stack>
#include <sigc++/sigc++.h>
#include <glibmm/ustring.h>

#include "DocumentInfo.h"
#include "CrawlHistory.h"
#include "MonitorInterface.h"
#include "MonitorHandler.h"
#include "QueryProperties.h"
#include "DaemonState.h"
#include "WorkerThreads.h"

class DirectoryScannerThread : public IndexingThread
{
	public:
		DirectoryScannerThread(const std::string &dirName, bool isSource,
			bool fullScan, bool isReindex,
			MonitorInterface *pMonitor, MonitorHandler *pHandler,
			unsigned int maxLevel = 0, bool followSymLinks = true);
		virtual ~DirectoryScannerThread();

		virtual std::string getType(void) const;

		virtual std::string getDirectory(void) const;

		virtual void stop(void);

		sigc::signal2<void, DocumentInfo, bool>& getFileFoundSignal(void);

	protected:
		std::string m_dirName;
		bool m_fullScan;
		bool m_isReindex;
		MonitorInterface *m_pMonitor;
		MonitorHandler *m_pHandler;
		unsigned int m_sourceId;
		unsigned int m_currentLevel;
		unsigned int m_maxLevel;
		bool m_followSymLinks;
		bool m_delegateIndexing;
		sigc::signal2<void, DocumentInfo, bool> m_signalFileFound;
		std::map<std::string, time_t> m_updateCache;
		std::stack<std::string> m_currentLinks;
		std::stack<std::string> m_currentLinkReferrees;

		void cacheUpdate(const std::string &location, time_t mTime, CrawlHistory &crawlHistory);
		void flushUpdates(CrawlHistory &crawlHistory);
		void foundFile(const DocumentInfo &docInfo);
		bool isIndexable(const std::string &entryName) const;
		bool scanEntry(const std::string &entryName, CrawlHistory &crawlHistory,
			bool statLinks = true);
		virtual void doWork(void);

	private:
		DirectoryScannerThread(const DirectoryScannerThread &other);
		DirectoryScannerThread &operator=(const DirectoryScannerThread &other);

};

#ifdef HAVE_DBUS
class DBusServletThread : public WorkerThread
{
	public:
		DBusServletThread(DaemonState *pServer, DBusServletInfo *pInfo);
		virtual ~DBusServletThread();

		virtual std::string getType(void) const;

		DBusServletInfo *getServletInfo(void) const;

		bool mustQuit(void) const;

	protected:
		DaemonState *m_pServer;
		DBusServletInfo *m_pServletInfo;
		bool m_mustQuit;

		virtual void doWork(void);

	private:
		DBusServletThread(const DBusServletThread &other);
		DBusServletThread &operator=(const DBusServletThread &other);

};
#endif

#endif // _SERVERTHREADS_HH
