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

#ifndef _MONITOR_EVENT_H
#define _MONITOR_EVENT_H

#include <string>

class MonitorEvent
{
	public:
		MonitorEvent();
		MonitorEvent(const MonitorEvent &other);
		virtual ~MonitorEvent();

		MonitorEvent& operator=(const MonitorEvent& other);

		bool operator<(const MonitorEvent& other) const;

		typedef enum { UNKNOWN = 0, EXISTS, CREATED, WRITE_CLOSED, MOVED, DELETED } EventType;

		std::string m_location;
		std::string m_previousLocation;
		bool m_isWatch;
		EventType m_type;
		bool m_isDirectory;

};

#endif // _MONITOR_EVENT_H
