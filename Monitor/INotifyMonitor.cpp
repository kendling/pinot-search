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

#include <sys/inotify.h>
#include <iostream>

#include "INotifyMonitor.h"

using std::string;
using std::map;
using std::set;
using std::pair;
using std::cout;
using std::cerr;
using std::endl;

INotifyMonitor::INotifyMonitor() :
	MonitorInterface()
{
	m_monitorFd = inotify_init();
}

INotifyMonitor::~INotifyMonitor()
{
	close(m_monitorFd);
}

/// Starts monitoring a location.
bool INotifyMonitor::addLocation(const string &directory)
{
	uint32_t eventsMask = IN_CLOSE_WRITE|IN_MOVE|IN_CREATE|IN_DELETE|IN_DELETE_SELF|IN_MOVE_SELF;

	if ((directory.empty() == true) ||
		(directory == "/") ||
		(m_monitorFd < 0))
	{
		return false;
	}

	map<string, int>::iterator watchIter = m_watches.find(directory);
	if (watchIter != m_watches.end())
	{
		// This is already being monitored
		return true;
	}

	// FIXME: check the maximum number of watches hasn't been reached (MAX_FILE_WATCHES ?)
	int dirWd = inotify_add_watch(m_monitorFd, directory.c_str(), eventsMask);
	if (dirWd >= 0)
	{
		m_watches.insert(pair<string, int>(directory, dirWd));

		return true;
	}
	cerr << "INotifyMonitor::addLocation: couldn't monitor " << directory << endl;

	return false;
}

/// Stops monitoring a location.
bool INotifyMonitor::removeLocation(const string &directory)
{
	if ((directory.empty() == true) ||
		(m_monitorFd < 0))
	{
		return false;
	}

	map<string, int>::iterator watchIter = m_watches.find(directory);
	if (watchIter != m_watches.end())
	{
		inotify_rm_watch(m_monitorFd, watchIter->second);
		m_watches.erase(watchIter);

		return true;
	}
	cerr << "INotifyMonitor::removeLocation: " << directory << " is not being monitored" << endl;

	return false;
}

/// Retrieves pending events.
bool INotifyMonitor::retrievePendingEvents(set<MonitorEvent> &events)
{
	char buffer[1024];
	size_t offset = 0;

	events.clear();
	if (m_monitorFd < 0)
	{
		return false;
	}

#ifdef DEBUG
	cout << "INotifyMonitor::retrievePendingEvents: reading events" << endl;
#endif
	int bytesRead = read(m_monitorFd, buffer, 1024); 
	while (bytesRead - offset > 0)
	{
		struct inotify_event *pEvent = (struct inotify_event *)&buffer[offset];
		size_t eventSize = sizeof(struct inotify_event) + pEvent->len;

		// The length includes the terminating NULL
		if (pEvent->len < 1)
		{
#ifdef DEBUG
			cout << "INotifyMonitor::retrievePendingEvents: no name for watch "
				<< pEvent->wd << endl;
#endif
			offset += eventSize;
			continue;
		}
		MonitorEvent monEvent;
		map<uint32_t, string>::iterator iter = m_movedFrom.end();

		// Get the whole event structure
		if (pEvent->name != NULL)
		{
#ifdef DEBUG
			cout << "INotifyMonitor::retrievePendingEvents: event on "
				<< pEvent->name << endl;
#endif
			monEvent.m_location = pEvent->name;
		}
		if (pEvent->mask & IN_ISDIR)
		{
			monEvent.m_isDirectory = true;
		}

		// What type of event ?
		switch (pEvent->mask)
		{
			case IN_CREATE:
				// Skip regular files
				if (monEvent.m_isDirectory == true)
				{
					monEvent.m_type = MonitorEvent::CREATED;
				}
				break;
			case IN_CLOSE_WRITE:
				monEvent.m_type = MonitorEvent::WRITE_CLOSED;
				break;
			case IN_MOVED_FROM:
				// Store this until we receive a IN_MOVED_TO event
				m_movedFrom.insert(pair<uint32_t, string>(pEvent->cookie, monEvent.m_location));
				break;
			case IN_MOVED_TO:
				// What was the previous location ?
				iter = m_movedFrom.find(pEvent->cookie);
				if (iter != m_movedFrom.end())
				{
					monEvent.m_previousLocation = iter->second;
					monEvent.m_type = MonitorEvent::MOVED;

					if (monEvent.m_isDirectory == true)
					{
						// We can already stop watching the old directory
						if (removeLocation(monEvent.m_previousLocation) == true)
						{
							// ...and watch this one instead
							addLocation(monEvent.m_location);
						}
					}
					m_movedFrom.erase(iter);
				}
#ifdef DEBUG
				else cout << "INotifyMonitor::retrievePendingEvents: don't know where file was moved from" << endl;
#endif
				break;
			case IN_DELETE_SELF:
				monEvent.m_type = MonitorEvent::DELETED;
				break;
			default:
#ifdef DEBUG
				cout << "INotifyMonitor::retrievePendingEvents: ignoring event "
					<< pEvent->mask << endl;
#endif
				break;
		}

		// Return event ?
		if (monEvent.m_type != MonitorEvent::UNKNOWN)
		{
			events.insert(monEvent);
		}

		offset += eventSize;
	}

	return true;
}
