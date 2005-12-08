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

#define _XOPEN_SOURCE	// glibc2 needs this
#include <time.h>
#include <iostream>

#include "TimeConverter.h"

using std::cout;
using std::endl;

TimeConverter::TimeConverter()
{
}

/// Converts into an RFC 822 timestamp.
string TimeConverter::toTimestamp(time_t aTime, bool inGMTime)
{
	struct tm timeTm;

	if (((inGMTime == true) &&
		(gmtime_r(&aTime, &timeTm) != NULL)) ||
		(localtime_r(&aTime, &timeTm) != NULL))
	{
		char timeStr[64];

		if (strftime(timeStr, 64, "%a, %d %b %Y %H:%M:%S %Z", &timeTm) > 0)
		{
			return timeStr;
		}
	}

	return "";
}

/// Converts from a RFC 822 timestamp.
time_t TimeConverter::fromTimestamp(const string &timestamp, bool inGMTime)
{
	struct tm timeTm;
	string formatString;
#ifndef _STRPTIME_COPES_WITH_TIMEZONE
	bool fixTimezone = false;
#endif

	if (timestamp.empty() == true)
	{
		return 0;
	}

	// Find out if the date has an RFC-822/ISO 8601 time zone specification
	// or a time zone name
	// FIXME: it looks like strptime() can't diffentiate between %Z and %z
	// and that the value extracted with %z is ignored
	char *remainder = strptime(timestamp.c_str(), "%a, %d %b %Y %H:%M:%S ", &timeTm);
	if (remainder != NULL)
	{
		if ((remainder[0] == '+') ||
			(remainder[0] == '-'))
		{
#ifdef _STRPTIME_COPES_WITH_TIMEZONE
			formatString = "%a, %d %b %Y %H:%M:%S %z";
#else
			formatString = "%a, %d %b %Y %H:%M:%S ";
			fixTimezone = true;
#endif
		}
		else
		{
			formatString = "%a, %d %b %Y %H:%M:%S %Z";
		}
	}
	else
	{
		remainder = strptime(timestamp.c_str(), "%Y %b %d %H:%M:%S ", &timeTm);
		if (remainder == NULL)
		{
			// How is it formatted then ?
			return 0;
		}

		if ((remainder[0] == '+') ||
			(remainder[0] == '-'))
		{
#ifdef _STRPTIME_COPES_WITH_TIMEZONE
			formatString = "%Y %b %d %H:%M:%S %z";
#else
			formatString = "%Y %b %d %H:%M:%S ";
			fixTimezone = true;
#endif
		}
		else
		{
			formatString = "%Y %b %d %H:%M:%S %Z";
		}
	}

	if ((formatString.empty() == false) &&
		(strptime(timestamp.c_str(), formatString.c_str(), &timeTm) != NULL))
	{
		time_t gmTime = 0;

		if (inGMTime == true)
		{
			gmTime = timegm(&timeTm);
		}
		else
		{
			gmTime = timelocal(&timeTm);
		}

#ifndef _STRPTIME_COPES_WITH_TIMEZONE
		if ((fixTimezone == true) &&
			(remainder != NULL))
		{
			unsigned int tzDiff = 0;

			if ((sscanf(remainder + 1, "%u", &tzDiff) != 0) &&
				(tzDiff < 1200))
			{
				unsigned int hourDiff = tzDiff / 100;
				unsigned int minDiff = tzDiff % 100;

#ifdef DEBUG
				cout << "TimeConverter::fromTimestamp: diff is " << remainder[0] << hourDiff << ":" << minDiff << endl;
#endif
				if (remainder[0] == '+')
				{
					// Ahead of GMT
					gmTime -= (hourDiff * 3600) + (minDiff * 60);
				}
				else
				{
					// Behind GMT
					gmTime += (hourDiff * 3600) + (minDiff * 60);
				}
			}
		}
#endif
		return gmTime;
	}

	return 0;
}
