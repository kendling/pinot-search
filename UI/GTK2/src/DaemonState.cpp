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

#include <iostream>
#include <sigc++/class_slot.h>
#include <glibmm/ustring.h>
#include <glibmm/stringutils.h>
#include <glibmm/convert.h>
#include <glibmm/thread.h>

#include "Url.h"
#include "MonitorFactory.h"
#include "XapianIndex.h"
#include "DaemonState.h"
#include "MboxHandler.h"
#include "OnDiskHandler.h"
#include "PinotSettings.h"
#include "PinotUtils.h"
#include "WorkerThreads.h"

using namespace std;
using namespace Glib;

DaemonState::DaemonState() :
	ThreadsManager(PinotSettings::getInstance().m_daemonIndexLocation, 20),
	m_pMailMonitor(MonitorFactory::getMonitor()),
	m_pDiskMonitor(MonitorFactory::getMonitor()),
	m_pDiskHandler(NULL)
{
	m_onThreadEndSignal.connect(SigC::slot(*this, &DaemonState::on_thread_end));
}

DaemonState::~DaemonState()
{
	if (m_pDiskMonitor != NULL)
	{
		delete m_pDiskMonitor;
	}
	if (m_pMailMonitor != NULL)
	{
		delete m_pMailMonitor;
	}
}

bool DaemonState::crawlLocation(const string &locationToCrawl, bool monitor)
{
	DirectoryScannerThread *pScannerThread = NULL;

	if (locationToCrawl.empty() == true)
	{
		return false;
	}

	if (monitor == false)
	{
		// Monitoring is not necessary
		pScannerThread = new DirectoryScannerThread(locationToCrawl, 0, true, NULL, NULL);
	}
	else
	{
		pScannerThread = new DirectoryScannerThread(locationToCrawl, 0, true, m_pDiskMonitor, m_pDiskHandler);
	}
	pScannerThread->getFileFoundSignal().connect(SigC::slot(*this, &DaemonState::on_message_filefound));

	if (start_thread(pScannerThread) == true)
	{
		m_locationBeingCrawled = locationToCrawl;

		return true;
	}

	return false;
}

void DaemonState::start(void)
{
	// Disable implicit flushing after a change
	WorkerThread::immediateFlush(false);

	// Fire up the mail monitor thread now
	MboxHandler *pMboxHandler = new MboxHandler();
	// Connect to its update signal
	pMboxHandler->getUpdateSignal().connect(
		SigC::slot(*this, &DaemonState::on_message_indexupdate));
	MonitorThread *pMailMonitorThread = new MonitorThread(m_pMailMonitor, pMboxHandler);
	pMailMonitorThread->getDirectoryFoundSignal().connect(SigC::slot(*this, &DaemonState::on_message_filefound));
	start_thread(pMailMonitorThread, true);

	// Same for the disk monitor thread
	m_pDiskHandler = new OnDiskHandler();
	// Connect to its update signal
	m_pDiskHandler->getUpdateSignal().connect(
		SigC::slot(*this, &DaemonState::on_message_indexupdate));
	MonitorThread *pDiskMonitorThread = new MonitorThread(m_pDiskMonitor, m_pDiskHandler);
	pDiskMonitorThread->getDirectoryFoundSignal().connect(SigC::slot(*this, &DaemonState::on_message_filefound));
	start_thread(pDiskMonitorThread, true);

	set<PinotSettings::IndexableLocation>::const_iterator locationIter = PinotSettings::getInstance().m_indexableLocations.begin();
	if (locationIter != PinotSettings::getInstance().m_indexableLocations.end())
	{
		// Crawl this now
		crawlLocation(locationIter->m_name, locationIter->m_monitor);
	}
}

void DaemonState::on_thread_end(WorkerThread *pThread)
{
	string indexedUrl;

	if (pThread == NULL)
	{
		return;
	}
#ifdef DEBUG
	cout << "DaemonState::on_thread_end: end of thread " << pThread->getId() << endl;
#endif

	// What type of thread was it ?
	string type(pThread->getType());
	if (type == "DirectoryScannerThread")
	{
		DirectoryScannerThread *pScannerThread = dynamic_cast<DirectoryScannerThread *>(pThread);
		if (pScannerThread == NULL)
		{
			delete pThread;
			return;
		}

		// Explicitely flush the index once a directory has been crawled
		XapianIndex index(PinotSettings::getInstance().m_daemonIndexLocation);
		index.flush();

		// Another location to crawl ?
		if ((m_locationBeingCrawled.empty() == false) &&
			(write_lock_lists() == true))
		{
			PinotSettings::IndexableLocation currentLocation;

			currentLocation.m_name = m_locationBeingCrawled;
			m_locationBeingCrawled.clear();

			set<PinotSettings::IndexableLocation>::const_iterator locationIter = PinotSettings::getInstance().m_indexableLocations.find(currentLocation);
			if (locationIter != PinotSettings::getInstance().m_indexableLocations.end())
			{
				// Get the next location
				++locationIter;
				if (locationIter != PinotSettings::getInstance().m_indexableLocations.end())
				{
					crawlLocation(locationIter->m_name, locationIter->m_monitor);
				}
			}

			unlock_lists();
		}
	}
	else if (type == "IndexingThread")
	{
		IndexingThread *pIndexThread = dynamic_cast<IndexingThread *>(pThread);
		if (pIndexThread == NULL)
		{
			delete pThread;
			return;
		}

		// Get the URL we have just indexed
		indexedUrl = pIndexThread->getURL();
	}
	else if (type == "UnindexingThread")
	{
		UnindexingThread *pUnindexThread = dynamic_cast<UnindexingThread *>(pThread);
		if (pUnindexThread == NULL)
		{
			delete pThread;
			return;
		}

		// FIXME: anything to do ?
	}
	else if (type == "MonitorThread")
	{
		// FIXME: do something about this
	}

	// Delete the thread
	delete pThread;;

	// We might be able to run a queued action
	pop_queue(indexedUrl);
}

void DaemonState::on_message_indexupdate(IndexedDocument docInfo, unsigned int docId, string indexName)
{
	// FIXME: anything to do ?
}

void DaemonState::on_message_filefound(const string &location, const string &sourceLabel, bool isDirectory)
{
	if (isDirectory == false)
	{
		Url urlObj(location);
		DocumentInfo docInfo(urlObj.getFile(), location, "", "");
		set<string> labels;

		// Insert a label that identifies the source
		labels.insert(sourceLabel);
		docInfo.setLabels(labels);

		queue_index(docInfo);
	}
	else
	{
		crawlLocation(location.substr(7), true);
#ifdef DEBUG
		cout << "DaemonState::on_message_filefound: new directory " << location.substr(7) << endl;
#endif
	}
}

