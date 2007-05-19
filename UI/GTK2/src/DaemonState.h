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

#include <string>
#include <queue>
#include <set>

#include "MonitorInterface.h"
#include "MonitorHandler.h"
#include "WorkerThreads.h"

class DaemonState : public ThreadsManager
{
	public:
		DaemonState();
		virtual ~DaemonState();

		void start(void);

		void signal_scanner(void);

		void on_thread_end(WorkerThread *pThread);

		void on_message_filefound(const DocumentInfo &docInfo, const std::string &sourceLabel,
			bool isDirectory);

		SigC::Signal1<void, int>& getQuitSignal(void);

	protected:
		bool m_fullScan;
		MonitorInterface *m_pMailMonitor;
		MonitorInterface *m_pDiskMonitor;
		MonitorHandler *m_pMailHandler;
		MonitorHandler *m_pDiskHandler;
		std::string m_locationBeingCrawled;
		SigC::Signal1<void, int> m_signalQuit;

		bool crawlLocation(const std::string &locationToCrawl, bool isSource, bool doMonitoring);

};

#endif
