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
#include "WritableXapianIndex.h"
#include "DaemonState.h"
#include "MboxHandler.h"
#include "OnDiskHandler.h"
#include "PinotSettings.h"
#include "PinotUtils.h"
#include "WorkerThreads.h"

using namespace std;
using namespace Glib;

DaemonState::DaemonState() :
	ThreadsManager(PinotSettings::getInstance().m_daemonIndexLocation, 10),
	m_crawling(false)
{
	m_onThreadEndSignal.connect(SigC::slot(*this, &DaemonState::on_thread_end));
}

DaemonState::~DaemonState()
{
}

void DaemonState::start(void)
{
	string locationToCrawl;

	// Disable implicit flushing after a change
	WorkerThread::immediateFlush(false);

	// Fire up the mail monitor thread now
	MboxHandler *pMbox = new MboxHandler();
	// Connect to its update signal
	pMbox->getUpdateSignal().connect(
		SigC::slot(*this, &DaemonState::on_message_indexupdate));
	MonitorThread *pMailMonitorThread = new MonitorThread(pMbox);
	start_thread(pMailMonitorThread, true);

	for (set<PinotSettings::TimestampedItem>::const_iterator locationIter = PinotSettings::getInstance().m_indexableLocations.begin();
		locationIter != PinotSettings::getInstance().m_indexableLocations.end(); ++locationIter)
	{
		if (locationToCrawl.empty() == true)
		{
			// Crawl this now
			locationToCrawl = locationIter->m_name;
		}
		else
		{
			// This will be crawled next
			m_crawlQueue.push(locationIter->m_name);
		}
	}

	// Anything to crawl before starting monitoring ?
	if (locationToCrawl.empty() == false)
	{
		// Scan the directory and import all its files
		DirectoryScannerThread *pScannerThread = new DirectoryScannerThread(locationToCrawl,
			0, true, &m_scanMutex, &m_scanCondVar);
		pScannerThread->getFileFoundSignal().connect(SigC::slot(*this, &DaemonState::on_message_filefound));

		m_crawling = start_thread(pScannerThread);
	}
	else
	{
		// Fire up the disk monitor thread right away
		OnDiskHandler *pDisk = new OnDiskHandler();
		// Connect to its update signal
		pDisk->getUpdateSignal().connect(
			SigC::slot(*this, &DaemonState::on_message_indexupdate));
		MonitorThread *pDiskMonitorThread = new MonitorThread(pDisk);
		start_thread(pDiskMonitorThread, true);
	}
}

void DaemonState::signal_scanner(void)
{
	// Ask the scanner for another file
	m_scanMutex.lock();
	m_scanCondVar.signal();
	m_scanMutex.unlock();
}

void DaemonState::on_thread_end(WorkerThread *pThread)
{
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

		DirectoryScannerThread *pNewScannerThread = NULL;

		// Another location to crawl ?
		if (write_lock_lists() == true)
		{
			if (m_crawlQueue.empty() == false)
			{
				string locationToCrawl(m_crawlQueue.front());

				set<PinotSettings::TimestampedItem> &indexableLocations = PinotSettings::getInstance().m_indexableLocations;
				for (set<PinotSettings::TimestampedItem>::iterator locationIter = indexableLocations.begin();
			                locationIter != indexableLocations.end(); ++locationIter)
				{
					if (locationIter->m_name == pScannerThread->getDirectory())
					{
						PinotSettings::TimestampedItem location(*locationIter);

						// Set the timestamp
						location.m_modTime = time(NULL);
						indexableLocations.erase(locationIter);
						indexableLocations.insert(location);

						break;
					}
				}

				pNewScannerThread = new DirectoryScannerThread(locationToCrawl,
					0, true, &m_scanMutex, &m_scanCondVar);
				pNewScannerThread->getFileFoundSignal().connect(SigC::slot(*this,
					&DaemonState::on_message_filefound));

				m_crawlQueue.pop();
			}
			else
			{
				// Done with crawling
				m_crawling = false;
			}

			unlock_lists();
		}

		// Explicitely flush the index once a directory has been crawled
		WritableXapianIndex index(PinotSettings::getInstance().m_daemonIndexLocation);
		index.flush();

		// Start a new scanner thread ?
		if (pNewScannerThread != NULL)
		{
			m_crawling = start_thread(pNewScannerThread);
		}
		else
		{
			// Now we can start monitoring
			OnDiskHandler *pDisk = new OnDiskHandler();
			// Connect to its update signal
			pDisk->getUpdateSignal().connect(
				SigC::slot(*this, &DaemonState::on_message_indexupdate));
			MonitorThread *pMonitorThread = new MonitorThread(pDisk);
			start_thread(pMonitorThread, true);
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

		// Did the thread perform an update ?
		if (pIndexThread->isNewDocument() == true)
		{
			string url(pIndexThread->getURL());

			// Update the in-progress list
			if (write_lock_lists() == true)
			{
				set<string>::iterator urlIter = m_beingIndexed.find(url);
				if (urlIter != m_beingIndexed.end())
				{
					m_beingIndexed.erase(urlIter);
				}

				unlock_lists();
			}
		}

		// Get another file from the directory scanner if possible
		if (m_crawling == true)
		{
			signal_scanner();
		}
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
	pop_queue();
}

void DaemonState::on_message_indexupdate(IndexedDocument docInfo, unsigned int docId, string indexName)
{
	// FIXME: anything to do ?
}

bool DaemonState::on_message_filefound(const string &location)
{
	Url urlObj(location);

	DocumentInfo docInfo(urlObj.getFile(), location, "", "");

	queue_index(docInfo);

	// Don't request another file right now
	return false;
}
