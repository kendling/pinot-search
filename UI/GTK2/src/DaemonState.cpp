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

#include "config.h"
#include <strings.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#ifdef HAVE_STATFS
  #ifdef HAVE_SYS_VFS_H
  #include <sys/vfs.h>
  #define CHECK_DISK_SPACE 1
  #else
    #ifdef HAVE_SYS_STATFS_H
      #include <sys/statfs.h>
      #define CHECK_DISK_SPACE 1
    #else
      #ifdef HAVE_SYS_MOUNT_H
        #include <sys/mount.h>
        #define CHECK_DISK_SPACE 1
      #endif
    #endif
  #endif
#else
  #ifdef HAVE_STATVFS
  #include <sys/statvfs.h>
  #define CHECK_DISK_SPACE 1
  #endif
#endif
#ifdef __FreeBSD__
#ifdef HAVE_SYSCTLBYNAME
#include <sys/sysctl.h>
#define CHECK_BATTERY_SYSCTL 1
#endif
#endif
#include <iostream>
#include <glibmm/ustring.h>
#include <glibmm/stringutils.h>
#include <glibmm/convert.h>
#include <glibmm/thread.h>
#include <glibmm/random.h>

#include "Url.h"
#include "MonitorFactory.h"
#include "CrawlHistory.h"
#include "DaemonState.h"
#include "OnDiskHandler.h"
#include "PinotSettings.h"
#include "PinotUtils.h"
#include "ServerThreads.h"

using namespace std;
using namespace Glib;

static double getFSFreeSpace(const string &path)
{
	double availableBlocks = 0.0;
	double blockSize = 0.0;
	int statSuccess = -1;
#ifdef HAVE_STATFS
	struct statfs fsStats;

	statSuccess = statfs(PinotSettings::getInstance().m_daemonIndexLocation.c_str(), &fsStats);
	availableBlocks = (uintmax_t)fsStats.f_bavail;
	blockSize = fsStats.f_bsize;
#else
#ifdef HAVE_STATVFS
	struct statvfs vfsStats;

	statSuccess = statvfs(path.c_str(), &vfsStats);
	availableBlocks = (uintmax_t)vfsStats.f_bavail;
	// f_frsize isn't supported by all implementations
	blockSize = (vfsStats.f_frsize ? vfsStats.f_frsize : vfsStats.f_bsize);
#endif
#endif
	// Did it fail ?
	if ((statSuccess == -1) ||
		(blockSize == 0.0))
	{
		return -1.0;
	}

	double mbRatio = blockSize / (1024 * 1024);
	double availableMbSize = availableBlocks * mbRatio;
#ifdef DEBUG
	cout << "DaemonState::getFSFreeSpace: " << availableBlocks << " blocks of " << blockSize
		<< " bytes (" << mbRatio << ")" << endl;
#endif

	return availableMbSize;
}

// A function object to stop DirectoryScanner threads with for_each()
struct StopScannerThreadFunc
{
public:
	void operator()(map<unsigned int, WorkerThread *>::value_type &p)
	{
		string type(p.second->getType());

		if (type == "DirectoryScannerThread")
		{
			p.second->stop();
#ifdef DEBUG
			cout << "StopScannerThreadFunc: stopped thread " << p.second->getId() << endl;
#endif
		}
	}
};

DaemonState::DaemonState() :
	ThreadsManager(PinotSettings::getInstance().m_daemonIndexLocation, 10),
	m_fullScan(false),
	m_reload(false),
	m_pDiskMonitor(MonitorFactory::getMonitor()),
	m_pDiskHandler(NULL),
	m_crawlers(0)
{
	FD_ZERO(&m_flagsSet);

	// Check disk usage every minute
	m_timeoutConnection = Glib::signal_timeout().connect(sigc::mem_fun(*this,
		&DaemonState::on_activity_timeout), 60000);
	// Check right now before doing anything else
	DaemonState::on_activity_timeout();

	m_onThreadEndSignal.connect(sigc::mem_fun(*this, &DaemonState::on_thread_end));
}

DaemonState::~DaemonState()
{
	// Don't delete m_pDiskMonitor and m_pDiskHandler, threads may need them
	// Since DaemonState is destroyed when the program exits, it's a leak we can live with
}

bool DaemonState::on_activity_timeout(void)
{
	if (m_timeoutConnection.blocked() == false)
	{
#ifdef CHECK_DISK_SPACE
		double availableMbSize = getFSFreeSpace(PinotSettings::getInstance().m_daemonIndexLocation);
		if (availableMbSize >= 0)
		{
#ifdef DEBUG
			cout << "DaemonState::on_activity_timeout: " << availableMbSize << " Mb free for "
				<< PinotSettings::getInstance().m_daemonIndexLocation << endl;
#endif
			if (availableMbSize < PinotSettings::getInstance().m_minimumDiskSpace)
			{
				// Stop indexing
				m_stopIndexing = true;
				// Stop crawling
				set_flag(LOW_DISK_SPACE);
				stop_crawling();

				cerr << "Stopped indexing because of low disk space" << endl;
			}
			else if (m_stopIndexing == true)
			{
				// Go ahead
				m_stopIndexing = false;
				reset_flag(LOW_DISK_SPACE);

				cerr << "Resumed indexing following low disk space condition" << endl;
			}
		}
#endif
#ifdef CHECK_BATTERY_SYSCTL
		// Check the battery state too
		check_battery_state();
#endif
	}

	return true;
}

void DaemonState::check_battery_state(void)
{
#ifdef CHECK_BATTERY_SYSCTL
	int acline = 1;
	size_t len = sizeof(acline);
	bool onBattery = false;

	// Are we on battery power ?
	if (sysctlbyname("hw.acpi.acline", &acline, &len, NULL, 0) == 0)
	{
#ifdef DEBUG
		cout << "DaemonState::check_battery_state: acline " << acline << endl;
#endif
		if (acline == 0)
		{
			onBattery = true;
		}

		bool wasOnBattery = is_flag_set(ON_BATTERY);
		if (onBattery != wasOnBattery)
		{
			if (onBattery == true)
			{
				// We are now on battery
				set_flag(ON_BATTERY);
				stop_crawling();

				cout << "System is now on battery" << endl;
			}
			else
			{
				// Back on-line
				reset_flag(ON_BATTERY);
				start_crawling();

				cout << "System is now on AC" << endl;
			}
		}
	}
#endif
}

bool DaemonState::crawl_location(const PinotSettings::IndexableLocation &location)
{
	string locationToCrawl(location.m_name);
	bool doMonitoring = location.m_monitor;
	bool isSource = location.m_isSource;
	DirectoryScannerThread *pScannerThread = NULL;

	// Can we go ahead and crawl ?
	if ((is_flag_set(LOW_DISK_SPACE) == true) ||
		(is_flag_set(ON_BATTERY) == true))
	{
#ifdef DEBUG
		cout << "DaemonState::crawl_location: crawling was stopped" << endl;
#endif
		return true;
	}

	if (locationToCrawl.empty() == true)
	{
		return false;
	}

	if (doMonitoring == false)
	{
		// Monitoring is not necessary, but we still have to pass the handler
		// so that we can act on documents that have been deleted
		pScannerThread = new DirectoryScannerThread(locationToCrawl, isSource, m_fullScan,
			NULL, m_pDiskHandler);
	}
	else
	{
		pScannerThread = new DirectoryScannerThread(locationToCrawl, isSource, m_fullScan,
			m_pDiskMonitor, m_pDiskHandler);
	}
	pScannerThread->getFileFoundSignal().connect(sigc::mem_fun(*this, &DaemonState::on_message_filefound));

	if (start_thread(pScannerThread, true) == true)
	{
		++m_crawlers;

		return true;
	}

	return false;
}

void DaemonState::start(bool forceFullScan)
{
	// Disable implicit flushing after a change
	WorkerThread::immediateFlush(false);

	// Do full scans ?
	if (forceFullScan == true)
	{
		m_fullScan = true;
	}
	else
	{
		Rand randomStuff;
		guint32 randomArray[5];

		randomStuff.set_seed(randomArray[2]);
		gint32 randomNum = randomStuff.get_int_range(0, 10);
		if (randomNum >= 7)
		{
			m_fullScan = true;
		}
#ifdef DEBUG
		cout << "DaemonState::start: picked " << randomNum << endl;
#endif
	}

	// Fire up the disk monitor thread
	if (m_pDiskHandler == NULL)
	{
		OnDiskHandler *pDiskHandler = new OnDiskHandler();
		pDiskHandler->getFileFoundSignal().connect(sigc::mem_fun(*this, &DaemonState::on_message_filefound));
		m_pDiskHandler = pDiskHandler;
	}
	MonitorThread *pDiskMonitorThread = new MonitorThread(m_pDiskMonitor, m_pDiskHandler);
	start_thread(pDiskMonitorThread, true);

	for (set<PinotSettings::IndexableLocation>::const_iterator locationIter = PinotSettings::getInstance().m_indexableLocations.begin();
		locationIter != PinotSettings::getInstance().m_indexableLocations.end(); ++locationIter)
	{
		m_crawlQueue.push(*locationIter);
	}
#ifdef DEBUG
	cout << "DaemonState::start: " << m_crawlQueue.size() << " locations to crawl" << endl;
#endif

	if (m_fullScan == true)
	{
		CrawlHistory crawlHistory(PinotSettings::getInstance().getHistoryDatabaseName());

		// Update all items status so that we can get rid of files from deleted sources
		crawlHistory.updateItemsStatus(CrawlHistory::CRAWLED, CrawlHistory::CRAWLING, 0, true);
	}
	// Initiate crawling
	start_crawling();
}

void DaemonState::reload(void)
{
	// Reload whenever possible
	m_reload = true;
}

void DaemonState::start_crawling(void)
{
	if (write_lock_lists() == true)
	{
#ifdef DEBUG
		cout << "DaemonState::start_crawling: " << m_crawlQueue.size() << " locations to crawl, "
			<< m_crawlers << " crawlers" << endl;
#endif
		// Get the next location, unless something is still being crawled
		if (m_crawlers == 0)
		{
			if (m_crawlQueue.empty() == false)
			{
				PinotSettings::IndexableLocation nextLocation(m_crawlQueue.front());

				crawl_location(nextLocation);
			}
			else if (m_fullScan == true)
			{
				CrawlHistory crawlHistory(PinotSettings::getInstance().getHistoryDatabaseName());
				set<string> deletedFiles;

				// All files left with status CRAWLING belong to deleted sources
				if ((m_pDiskHandler != NULL) &&
					(crawlHistory.getItems(CrawlHistory::CRAWLING, deletedFiles) > 0))
				{
#ifdef DEBUG
					cout << "DaemonState::start_crawling: " << deletedFiles.size() << " orphaned files" << endl;
#endif
					for(set<string>::const_iterator fileIter = deletedFiles.begin();
						fileIter != deletedFiles.end(); ++fileIter)
					{
						// Inform the MonitorHandler
						if (m_pDiskHandler->fileDeleted(fileIter->substr(7)) == true)
						{
							// Delete this item
							crawlHistory.deleteItem(*fileIter);
						}
					}
				}
			}
		}

		unlock_lists();
	}

}

void DaemonState::stop_crawling(void)
{
	if (write_lock_threads() == true)
	{
		if (m_threads.empty() == false)
		{
			// Stop all DirectoryScanner threads
			for_each(m_threads.begin(), m_threads.end(), StopScannerThreadFunc());
		}

		unlock_threads();
	}
}

void DaemonState::on_thread_end(WorkerThread *pThread)
{
	string indexedUrl;

	if (pThread == NULL)
	{
		return;
	}

	// What type of thread was it ?
	string type(pThread->getType());
#ifdef DEBUG
	cout << "DaemonState::on_thread_end: end of thread " << type << " " << pThread->getId() << endl;
#endif
	if (type == "DirectoryScannerThread")
	{
		DirectoryScannerThread *pScannerThread = dynamic_cast<DirectoryScannerThread *>(pThread);
		if (pScannerThread == NULL)
		{
			delete pThread;
			return;
		}
		--m_crawlers;
#ifdef DEBUG
		cout << "DaemonState::on_thread_end: done crawling " << pScannerThread->getDirectory() << endl;
#endif

		// Explicitely flush the index once a directory has been crawled
		IndexInterface *pIndex = PinotSettings::getInstance().getIndex(PinotSettings::getInstance().m_daemonIndexLocation);
		if (pIndex != NULL)
		{
			pIndex->flush();

			delete pIndex;
		}

		if (pScannerThread->isStopped() == false)
		{
			// Pop the queue
			m_crawlQueue.pop();
		}
		// Else, the directory wasn't fully crawled so better leave it in the queue

		start_crawling();
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

		// Did it fail ?
		int errorNum = pThread->getErrorNum();
		if ((errorNum > 0) &&
			(indexedUrl.empty() == false))
		{
			CrawlHistory crawlHistory(PinotSettings::getInstance().getHistoryDatabaseName());

			// An entry should already exist for this
			crawlHistory.updateItem(indexedUrl, CrawlHistory::ERROR, time(NULL), errorNum);
		}
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
			// Disconnect the timeout signal
			if (m_timeoutConnection.connected() == true)
			{
				m_timeoutConnection.block();
				m_timeoutConnection.disconnect();
			}
			m_signalQuit(0);
		}
	}

	// Delete the thread
	delete pThread;

	// Are we supposed to reload the configuration ?
	// Wait until there are no threads running (except background ones)
	if ((m_reload == true) &&
		(get_threads_count() == 0))
	{
#ifdef DEBUG
		cout << "DaemonState::on_thread_end: stopping all threads" << endl;
#endif
		// Stop background threads
		stop_threads();
		// ...clear the queues
		clear_queues();

		// Reload
		PinotSettings &settings = PinotSettings::getInstance();
		settings.clear();
		settings.loadGlobal(string(SYSCONFDIR) + "/pinot/globalconfig.xml");
		settings.load();
		m_reload = false;
#ifdef DEBUG
		cout << "DaemonState::on_thread_end: reloading" << endl;
#endif

		// ...and restart everything 
		start(true);
	}
#ifdef DEBUG
	cout << "DaemonState::on_thread_end: reload status " << m_reload << endl;
#endif

	// We might be able to run a queued action
	pop_queue(indexedUrl);
}

void DaemonState::on_message_filefound(DocumentInfo docInfo, string sourceLabel, bool isDirectory)
{
	if (isDirectory == false)
	{
		DocumentInfo docCopy(docInfo);
		set<string> labels;

		if (sourceLabel.empty() == false)
		{
			// Insert a label that identifies the source
			labels.insert(sourceLabel);
			docCopy.setLabels(labels);
#ifdef DEBUG
			cout << "DaemonState::on_message_filefound: source label for " << docCopy.getLocation() << " is " << sourceLabel << endl;
#endif
		}
#ifdef DEBUG
		else cout << "DaemonState::on_message_filefound: no source label for " << docCopy.getLocation() << endl;
#endif

		queue_index(docCopy);
	}
	else
	{
		PinotSettings::IndexableLocation newLocation;

		newLocation.m_monitor = true;
		newLocation.m_name = docInfo.getLocation().substr(7);
		newLocation.m_isSource = false;
#ifdef DEBUG
		cout << "DaemonState::on_message_filefound: new directory " << newLocation.m_name << endl;
#endif

		// Queue this directory for crawling
		m_crawlQueue.push(newLocation);
		start_crawling();
	}
}

sigc::signal1<void, int>& DaemonState::getQuitSignal(void)
{
	return m_signalQuit;
}

void DaemonState::set_flag(StatusFlag flag)
{
	FD_SET((int)flag, &m_flagsSet);
}

bool DaemonState::is_flag_set(StatusFlag flag)
{
	if (FD_ISSET((int)flag, &m_flagsSet))
	{
		return true;
	}

	return false;
}

void DaemonState::reset_flag(StatusFlag flag)
{
	FD_CLR((int)flag, &m_flagsSet);
}

