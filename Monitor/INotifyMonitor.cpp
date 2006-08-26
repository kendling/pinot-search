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

#include "config.h"
#include <sys/ioctl.h>
#ifdef HAVE_LINUX_INOTIFY_H
#include <stdint.h>
#include <linux/inotify.h>
#include "linux-inotify-syscalls.h"
#else
#ifdef HAVE_SYS_INOTIFY_H
#include <sys/inotify.h>
#endif
#endif
#include <string.h>
#include <errno.h>
#include <iostream>

#include "INotifyMonitor.h"

using std::string;
using std::map;
using std::queue;
using std::pair;
using std::cout;
using std::cerr;
using std::endl;

INotifyMonitor::INotifyMonitor() :
	MonitorInterface()
{
	m_monitorFd = inotify_init();
	if (m_monitorFd < 0)
	{
		char errBuffer[1024];

		strerror_r(errno, errBuffer, 1024);
		cerr << "INotifyMonitor: initialization failed: " << errBuffer << endl;
	}
}

INotifyMonitor::~INotifyMonitor()
{
	if (m_monitorFd >= 0)
	{
		close(m_monitorFd);
	}
}

/// Starts monitoring a location.
bool INotifyMonitor::addLocation(const string &location, bool isDirectory)
{
	uint32_t eventsMask = IN_CLOSE_WRITE|IN_MOVE|IN_CREATE|IN_DELETE|IN_UNMOUNT|IN_MOVE_SELF|IN_DELETE_SELF;

	if ((location.empty() == true) ||
		(location == "/") ||
		(m_monitorFd < 0))
	{
		return false;
	}

	map<string, int>::iterator locationIter = m_locations.find(location);
	if (locationIter != m_locations.end())
	{
		// This is already being monitored
		return true;
	}

	// FIXME: check the maximum number of watches hasn't been reached (MAX_FILE_WATCHES ?)
	int watchNum = inotify_add_watch(m_monitorFd, location.c_str(), eventsMask);
	if (watchNum >= 0)
	{
		MonitorEvent monEvent;

		// Generate an event to signal it exists and is being monitored
		monEvent.m_location = location;
		monEvent.m_isWatch = true;
		monEvent.m_type = MonitorEvent::EXISTS;
		monEvent.m_isDirectory = isDirectory;
		m_internalEvents.push(monEvent);

		m_watches.insert(pair<int, string>(watchNum, location));
		m_locations.insert(pair<string, int>(location, watchNum));
#ifdef DEBUG
		cout << "INotifyMonitor::addLocation: added watch "
			<< watchNum << " for " << location << endl;
#endif

		return true;
	}
	cerr << "INotifyMonitor::addLocation: couldn't monitor " << location << endl;

	return false;
}

/// Stops monitoring a location.
bool INotifyMonitor::removeLocation(const string &location)
{
	if ((location.empty() == true) ||
		(m_monitorFd < 0))
	{
		return false;
	}

	map<string, int>::iterator locationIter = m_locations.find(location);
	if (locationIter != m_locations.end())
	{
		inotify_rm_watch(m_monitorFd, locationIter->second);
		map<int, string>::iterator watchIter = m_watches.find(locationIter->second);
		if (watchIter != m_watches.end())
		{
			m_watches.erase(watchIter);
		}
		m_locations.erase(locationIter);

		return true;
	}
	cerr << "INotifyMonitor::removeLocation: " << location << " is not being monitored" << endl;

	return false;
}

/// Retrieves pending events.
bool INotifyMonitor::retrievePendingEvents(queue<MonitorEvent> &events)
{
	char buffer[1024];
	unsigned int queueLen;
	size_t offset = 0;

	if (m_monitorFd < 0)
	{
		return false;
	}

	// Copy internal events
	while (m_internalEvents.empty() == false)
	{
		MonitorEvent &internalEvent = m_internalEvents.front();
		events.push(internalEvent);

		// Next
		m_internalEvents.pop();
	}

	if (ioctl(m_monitorFd, FIONREAD, &queueLen) == 0)
	{
#ifdef DEBUG
		cout << "INotifyMonitor::retrievePendingEvents: "
			<< queueLen << " bytes to read" << endl;
#endif
		if (queueLen == 0)
		{
			// Nothing to read
			return true;
		}
	}

	int bytesRead = read(m_monitorFd, buffer, 1024);
	while ((bytesRead > 0) &&
		(bytesRead - offset > 0))
	{
		struct inotify_event *pEvent = (struct inotify_event *)&buffer[offset];
		size_t eventSize = sizeof(struct inotify_event) + pEvent->len;

#ifdef DEBUG
		cout << "INotifyMonitor::retrievePendingEvents: read "
			<< bytesRead << " bytes at offset " << offset << endl;
#endif
		// What location is this event for ?
		map<int, string>::iterator watchIter = m_watches.find(pEvent->wd);
		if (watchIter == m_watches.end())
		{
#ifdef DEBUG
			cout << "INotifyMonitor::retrievePendingEvents: unknown watch "
				<< pEvent->wd << endl;
#endif
			offset += eventSize;
			continue;
		}

		MonitorEvent monEvent;

		monEvent.m_isWatch = true;
		if (pEvent->mask & IN_ISDIR)
		{
			monEvent.m_isDirectory = true;
		}

		monEvent.m_location = watchIter->second;
		// A name is provided if the target is below a location we match
		if (pEvent->len >= 1)
		{
			monEvent.m_location += "/";
			monEvent.m_location += pEvent->name;
			monEvent.m_isWatch = false;
		}
#ifdef DEBUG
		cout << "INotifyMonitor::retrievePendingEvents: event on "
			<< monEvent.m_location << endl;
#endif

		map<uint32_t, string>::iterator movedIter = m_movedFrom.end();

		// What type of event ?
		if (pEvent->mask & IN_CREATE)
		{
			// Skip regular files
			if (monEvent.m_isDirectory == true)
			{
				monEvent.m_type = MonitorEvent::CREATED;
			}
		}
		else if (pEvent->mask & IN_CLOSE_WRITE)
		{
			monEvent.m_type = MonitorEvent::WRITE_CLOSED;
		}
		else if (pEvent->mask & IN_MOVED_FROM)
		{
			// Store this until we receive a IN_MOVED_TO event
			m_movedFrom.insert(pair<uint32_t, string>(pEvent->cookie, monEvent.m_location));
		}
		else if (pEvent->mask & IN_MOVED_TO)
		{
			// What was the previous location ?
			movedIter = m_movedFrom.find(pEvent->cookie);
			if (movedIter != m_movedFrom.end())
			{
				monEvent.m_previousLocation = movedIter->second;
				monEvent.m_type = MonitorEvent::MOVED;
#ifdef DEBUG
				cout << "INotifyMonitor::retrievePendingEvents: moved from "
					<< monEvent.m_previousLocation << endl;
#endif
				m_movedFrom.erase(movedIter);
			}
#ifdef DEBUG
			else cout << "INotifyMonitor::retrievePendingEvents: don't know where file was moved from" << endl;
#endif
		}
		else if (pEvent->mask & IN_DELETE)
		{
			monEvent.m_type = MonitorEvent::DELETED;
		}
		else if (pEvent->mask & IN_MOVE_SELF)
		{
			// FIXME: how do we find out where the watched location was moved to ?
		}
		else if (pEvent->mask & IN_DELETE_SELF)
		{
#ifdef DEBUG
			cout << "INotifyMonitor::retrievePendingEvents: watch moved or deleted itself" << endl;
#endif
			if (monEvent.m_isWatch == true)
			{
				removeLocation(monEvent.m_location);
			}
		}
		else if (pEvent->mask & IN_UNMOUNT)
		{
			if (monEvent.m_isWatch == true)
			{
				// Watches are removed silently if the backing filesystem is unmounted
				removeLocation(monEvent.m_location);
			}
		}
		else
		{
#ifdef DEBUG
			cout << "INotifyMonitor::retrievePendingEvents: ignoring event "
				<< pEvent->mask << endl;
#endif
		}

		// Return event ?
		if (monEvent.m_type != MonitorEvent::UNKNOWN)
		{
			events.push(monEvent);
		}

		offset += eventSize;
	}

	return true;
}
