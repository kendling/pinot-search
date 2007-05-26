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

#ifndef _SERVERTHREADS_HH
#define _SERVERTHREADS_HH

#include <string>
#include <vector>
extern "C"
{
#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
}
#include <sigc++/object.h>
#include <sigc++/slot.h>
#include <sigc++/connection.h>
#include <glibmm/ustring.h>

#include "DocumentInfo.h"
#include "CrawlHistory.h"
#include "MonitorInterface.h"
#include "MonitorHandler.h"
#include "QueryProperties.h"
#include "DaemonState.h"
#include "WorkerThreads.h"

class MonitorThread : public WorkerThread
{
	public:
		MonitorThread(MonitorInterface *pMonitor, MonitorHandler *pHandler);
		virtual ~MonitorThread();

		virtual std::string getType(void) const;

		virtual bool stop(void);

		SigC::Signal3<void, const DocumentInfo&, const std::string&, bool>& getDirectoryFoundSignal(void);

	protected:
		int m_ctrlReadPipe;
		int m_ctrlWritePipe;
		MonitorInterface *m_pMonitor;
		MonitorHandler *m_pHandler;
		SigC::Signal3<void, const DocumentInfo&, const std::string&, bool> m_signalDirectoryFound;

		void processEvents(void);
		virtual void doWork(void);

	private:
		MonitorThread(const MonitorThread &other);
		MonitorThread &operator=(const MonitorThread &other);

};

class DirectoryScannerThread : public WorkerThread
{
	public:
		DirectoryScannerThread(const std::string &dirName, bool isSource,
			bool fullScan, MonitorInterface *pMonitor, MonitorHandler *pHandler,
			unsigned int maxLevel = 0, bool followSymLinks = true);
		virtual ~DirectoryScannerThread();

		virtual std::string getType(void) const;

		virtual std::string getDirectory(void) const;

		virtual bool stop(void);

		SigC::Signal3<void, const DocumentInfo&, const std::string&, bool>& getFileFoundSignal(void);

	protected:
		std::string m_dirName;
		bool m_fullScan;
		MonitorInterface *m_pMonitor;
		MonitorHandler *m_pHandler;
		unsigned int m_sourceId;
		unsigned int m_currentLevel;
		unsigned int m_maxLevel;
		bool m_followSymLinks;
		SigC::Signal3<void, const DocumentInfo&, const std::string&, bool> m_signalFileFound;
		std::map<std::string, time_t> m_updateCache;

		void cacheUpdate(const std::string &location, time_t mTime, CrawlHistory &history);
		void flushUpdates(CrawlHistory &history);
		void foundFile(const DocumentInfo &docInfo);
		bool scanEntry(const std::string &entryName, CrawlHistory &history);
		virtual void doWork(void);

	private:
		DirectoryScannerThread(const DirectoryScannerThread &other);
		DirectoryScannerThread &operator=(const DirectoryScannerThread &other);

};

class DBusServletThread : public WorkerThread
{
	public:
		DBusServletThread(DaemonState *pServer, DBusConnection *pConnection, DBusMessage *pRequest);
		virtual ~DBusServletThread();

		virtual std::string getType(void) const;

		virtual bool stop(void);

		DBusConnection *getConnection(void) const;

		DBusMessage *getReply(void) const;

		bool mustQuit(void) const;

	protected:
		DaemonState *m_pServer;
		DBusConnection *m_pConnection;
		DBusMessage *m_pRequest;
		DBusMessage *m_pReply;
		GPtrArray *m_pArray;
		bool m_mustQuit;

		bool runQuery(QueryProperties &queryProps, std::vector<std::string> &docIds);

		virtual void doWork(void);

	private:
		DBusServletThread(const DBusServletThread &other);
		DBusServletThread &operator=(const DBusServletThread &other);

};

#endif // _SERVERTHREADS_HH
