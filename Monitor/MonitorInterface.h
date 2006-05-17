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

#ifndef _MONITOR_INTERFACE_H
#define _MONITOR_INTERFACE_H

#include <string>
#include <map>
#include <set>

#include "MonitorEvent.h"

class MonitorInterface
{
	public:
		virtual ~MonitorInterface()
		{
		}

		/// Returns the file descriptor to poll for events.
		virtual int getFileDescriptor(void) const
		{
			return m_monitorFd;
		}

		/// Adds a watch for the specified location.
		virtual bool addLocation(const std::string &directory) = 0;

		/// Removed the watch for the specified location.
		virtual bool removeLocation(const std::string &directory) = 0;

		/// Retrieves pending events.
		virtual bool retrievePendingEvents(std::set<MonitorEvent> &events) = 0;

	protected:
		int m_monitorFd;

		MonitorInterface() :
			m_monitorFd(-1)
		{
		}

	private:
		MonitorInterface(const MonitorInterface &other);
		MonitorInterface &operator=(const MonitorInterface &other);

};

#endif // _MONITOR_INTERFACE_H
