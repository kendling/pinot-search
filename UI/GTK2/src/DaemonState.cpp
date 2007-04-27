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

#include <iostream>
#include <sigc++/class_slot.h>
#include <sigc++/compatibility.h>
#include <sigc++/slot.h>
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
#include "ServerThreads.h"

using namespace std;
using namespace Glib;

DaemonState::DaemonState() :
	ThreadsManager(PinotSettings::getInstance().m_daemonIndexLocation, 20),
	m_pMailMonitor(MonitorFactory::getMonitor()),
	m_pDiskMonitor(MonitorFactory::getMonitor()),
	m_pMailHandler(NULL),
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
	// Don't delete m_pDiskHandler and m_pMailHandler, threads may need them
	// Since DaemonState is destroyed when the program exits, it's okay
}

bool DaemonState::crawlLocation(const string &locationToCrawl, bool isSource, bool doMonitoring)
{
	DirectoryScannerThread *pScannerThread = NULL;

	if (locationToCrawl.empty() == true)
	{
		return false;
	}

	if (doMonitoring == false)
	{
		// Monitoring is not necessary, but we still have to pass the handler
		// so that we can act on documents that have been deleted
		pScannerThread = new DirectoryScannerThread(locationToCrawl, isSource,
			0, true, NULL, m_pDiskHandler);
	}
	else
	{
		pScannerThread = new DirectoryScannerThread(locationToCrawl, isSource,
			0, true, m_pDiskMonitor, m_pDiskHandler);
	}
	pScannerThread->getFileFoundSignal().connect(SigC::slot(*this, &DaemonState::on_message_filefound));

	if (start_thread(pScannerThread) == true)
	{
		m_locationBeingCrawled = locationToCrawl;

		return true;
	}

	return false;
}

void DaemonState::start(bool doUpgrade)
{
	// Disable implicit flushing after a change
	WorkerThread::immediateFlush(false);

	// Reset the index ?
	if (doUpgrade == true)
	{
		XapianIndex daemonIndex(PinotSettings::getInstance().m_daemonIndexLocation);

		if (daemonIndex.unindexAllDocuments() == true)
		{
			CrawlHistory history(PinotSettings::getInstance().m_historyDatabase);
			map<unsigned int, string> sources;

			// ...and the history
			history.getSources(sources);
			for (std::map<unsigned int, string>::iterator sourceIter = sources.begin();
				sourceIter != sources.end(); ++sourceIter)
			{
				history.deleteItems(sourceIter->first);
				history.deleteSource(sourceIter->first);
			}
		}
	}

	// Fire up the mail monitor thread now
	m_pMailHandler = new MboxHandler();
	MonitorThread *pMailMonitorThread = new MonitorThread(m_pMailMonitor, m_pMailHandler);
	pMailMonitorThread->getDirectoryFoundSignal().connect(SigC::slot(*this, &DaemonState::on_message_filefound));
	start_thread(pMailMonitorThread, true);

	// Same for the disk monitor thread
	m_pDiskHandler = new OnDiskHandler();
	MonitorThread *pDiskMonitorThread = new MonitorThread(m_pDiskMonitor, m_pDiskHandler);
	pDiskMonitorThread->getDirectoryFoundSignal().connect(SigC::slot(*this, &DaemonState::on_message_filefound));
	start_thread(pDiskMonitorThread, true);

	set<PinotSettings::IndexableLocation>::const_iterator locationIter = PinotSettings::getInstance().m_indexableLocations.begin();
	if (locationIter != PinotSettings::getInstance().m_indexableLocations.end())
	{
		// Crawl this now
		crawlLocation(locationIter->m_name, true, locationIter->m_monitor);
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
					crawlLocation(locationIter->m_name, true, locationIter->m_monitor);
				}
			}
#ifdef DEBUG
			else cout << "DaemonState::on_thread_end: nothing else to crawl" << endl;
#endif

			unlock_lists();
		}
#ifdef DEBUG
		else cout << "DaemonState::on_thread_end: done crawling" << endl;
#endif
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
		// FIXME: anything to do ?
	}
	else if (type == "MonitorThread")
	{
		// FIXME: do something about this
	}
	else if (type == "DBusServletThread")
	{
		DBusServletThread *pDBusThread = dynamic_cast<DBusServletThread *>(pThread);
		if (pDBusThread == NULL)
		{
			delete pThread;
			return;
		}

		if (pDBusThread->mustQuit() == true)
		{
			m_signalQuit(0);
		}
	}

	// Delete the thread
	delete pThread;

	// We might be able to run a queued action
	pop_queue(indexedUrl);
}

void DaemonState::on_message_filefound(const DocumentInfo &docInfo, const string &sourceLabel, bool isDirectory)
{
	if (isDirectory == false)
	{
		DocumentInfo docCopy(docInfo);
		set<string> labels;

		// Insert a label that identifies the source
		labels.insert(sourceLabel);
		docCopy.setLabels(labels);

		queue_index(docCopy);
	}
	else
	{
		string location(docInfo.getLocation());

		crawlLocation(location.substr(7), false, true);
#ifdef DEBUG
		cout << "DaemonState::on_message_filefound: new directory " << location.substr(7) << endl;
#endif
	}
}

SigC::Signal1<void, int>& DaemonState::getQuitSignal(void)
{
	return m_signalQuit;
}

