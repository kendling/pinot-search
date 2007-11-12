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

#ifndef _DBUSERVER_HH
#define _DBUSERVER_HH

#include <sys/select.h>
#include <string>
#include <queue>
#include <set>
#include <sigc++/sigc++.h>

#include "MonitorInterface.h"
#include "MonitorHandler.h"
#include "WorkerThreads.h"

class DaemonState : public ThreadsManager
{
	public:
		DaemonState();
		virtual ~DaemonState();

		typedef enum { LOW_DISK_SPACE = 0, ON_BATTERY } StatusFlag;

		void start(bool forceFullScan);

		void reload(void);

		void stop_crawling(void);

		void on_thread_end(WorkerThread *pThread);

		void on_message_filefound(const DocumentInfo &docInfo, const std::string &sourceLabel,
			bool isDirectory);

		sigc::signal1<void, int>& getQuitSignal(void);

		void set_flag(StatusFlag flag);

		bool is_flag_set(StatusFlag flag);

		void reset_flag(StatusFlag flag);

	protected:
		bool m_fullScan;
		bool m_reload;
		fd_set m_flagsSet;
		MonitorInterface *m_pBatteryMonitor;
		MonitorInterface *m_pDiskMonitor;
		MonitorHandler *m_pDiskHandler;
		MonitorHandler *m_pBatteryHandler;
		sigc::connection m_timeoutConnection;
		sigc::signal1<void, int> m_signalQuit;

		bool on_activity_timeout(void);

		bool crawl_location(const std::string &locationToCrawl, bool isSource, bool doMonitoring);

};

#endif
