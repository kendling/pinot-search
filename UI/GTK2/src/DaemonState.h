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

#ifndef _DBUSERVER_HH
#define _DBUSERVER_HH

#include <string>
#include <queue>
#include <set>
#include <glibmm/thread.h>

#include "MonitorInterface.h"
#include "IndexedDocument.h"
#include "WorkerThreads.h"

class DaemonState : public ThreadsManager
{
	public:
		DaemonState();
		virtual ~DaemonState();

		void start(void);

		void signal_scanner(void);

		void on_thread_end(WorkerThread *pThread);

		void on_message_indexupdate(IndexedDocument docInfo, unsigned int docId, std::string indexName);

		bool on_message_filefound(const std::string &location, const std::string &sourceLabel);

	protected:
		MonitorInterface *m_pMailMonitor;
		MonitorInterface *m_pDiskMonitor;
		std::queue<std::string> m_crawlQueue;
		std::set<std::string> m_monitoredLocations;
		bool m_crawling;
		Glib::Mutex m_scanMutex;
		Glib::Cond m_scanCondVar;

};

#endif
