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
#ifdef HAVE_TIMEGM
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#include <time.h>
#undef _XOPEN_SOURCE
#else
#include <time.h>
#endif
#else
#include <time.h>
/* This comment and function are taken from Wget's mktime_from_utc()
   Converts struct tm to time_t, assuming the data in tm is UTC rather
   than local timezone.

   mktime is similar but assumes struct tm, also known as the
   "broken-down" form of time, is in local time zone.  mktime_from_utc
   uses mktime to make the conversion understanding that an offset
   will be introduced by the local time assumption.

   mktime_from_utc then measures the introduced offset by applying
   gmtime to the initial result and applying mktime to the resulting
   "broken-down" form.  The difference between the two mktime results
   is the measured offset which is then subtracted from the initial
   mktime result to yield a calendar time which is the value returned.

   tm_isdst in struct tm is set to 0 to force mktime to introduce a
   consistent offset (the non DST offset) since tm and tm+o might be
   on opposite sides of a DST change.

   Some implementations of mktime return -1 for the nonexistent
   localtime hour at the beginning of DST.  In this event, use
   mktime(tm - 1hr) + 3600.

   Schematically
     mktime(tm)   --> t+o
     gmtime(t+o)  --> tm+o
     mktime(tm+o) --> t+2o
     t+o - (t+2o - t+o) = t

   Note that glibc contains a function of the same purpose named
   `timegm' (reverse of gmtime).  But obviously, it is not universally
   available, and unfortunately it is not straightforwardly
   extractable for use here.  Perhaps configure should detect timegm
   and use it where available.

   Contributed by Roger Beeman <beeman@cisco.com>, with the help of
   Mark Baushke <mdb@cisco.com> and the rest of the Gurus at CISCO.
   Further improved by Roger with assistance from Edward J. Sabol
   based on input by Jamie Zawinski.  */
static time_t mktime_from_utc (struct tm *t)
{
  time_t tl, tb;
  struct tm *tg;

  tl = mktime (t);
  if (tl == -1)
    {
      t->tm_hour--;
      tl = mktime (t);
      if (tl == -1)
	return -1; /* can't deal with output from strptime */
      tl += 3600;
    }
  tg = gmtime (&tl);
  tg->tm_isdst = 0;
  tb = mktime (tg);
  if (tb == -1)
    {
      tg->tm_hour--;
      tb = mktime (tg);
      if (tb == -1)
	return -1; /* can't deal with output from gmtime */
      tb += 3600;
    }
  return (tl - (tb - tl));
}
#define timegm(T) mktime_from_utc(T)
#endif
#include <iostream>

#include "TimeConverter.h"

using std::cout;
using std::endl;
using std::string;

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

	// Initialize the structure
	timeTm.tm_sec = timeTm.tm_min = timeTm.tm_hour = timeTm.tm_mday = 0;
	timeTm.tm_mon = timeTm.tm_year = timeTm.tm_wday = timeTm.tm_yday = timeTm.tm_isdst = 0;

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
			gmTime = mktime(&timeTm);
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

/// Converts to a YYYYMMDD-formatted string.
string TimeConverter::toYYYYMMDDString(int year, int month, int day)
{
	char dateStr[64];

	if (year < 0)
	{
		year = 0;
	}
	else if (year > 9999)
	{
		year = 9999;
	}
	if (month < 1)
	{
		month = 1;
	}
	else if (month > 12)
	{
		month = 12;
	}
	if (day < 1)
	{
		day = 1;
	}
	else if (day > 31)
	{
		day = 31;
	}

	if (snprintf(dateStr, 63, "%04d%02d%02d", year, month, day) > 0)
	{
		return dateStr;
	}

	return "";
}

/// Converts from a YYYYMMDD-formatted string.
time_t TimeConverter::fromYYYYMMDDString(const string &yyyymmdd, bool inGMTime)
{
	struct tm timeTm;
	time_t gmTime = 0;

	// Initialize the structure
	timeTm.tm_sec = timeTm.tm_min = timeTm.tm_hour = timeTm.tm_mday = 0;
	timeTm.tm_mon = timeTm.tm_year = timeTm.tm_wday = timeTm.tm_yday = timeTm.tm_isdst = 0;

	strptime(yyyymmdd.c_str(), "%Y%m%d", &timeTm);
#ifdef DEBUG
	cout << "TimeConverter::fromYYYYMMDDString: " << timeTm.tm_year << " " << timeTm.tm_mon << " " << timeTm.tm_mday << endl;
#endif
	if (inGMTime == true)
	{
		gmTime = timegm(&timeTm);
	}
	else
	{
		gmTime = mktime(&timeTm);
	}

	return gmTime;
}

/// Converts to a HHMMSS-formatted string.
string TimeConverter::toHHMMSSString(int hours, int minutes, int seconds)
{
	char timeStr[64];

	if (hours < 0)
	{
		hours = 0;
	}
	else if (hours > 23)
	{
		hours = 23;
	}
	if (minutes < 0)
	{
		minutes = 0;
	}
	else if (minutes > 59)
	{
		minutes = 59;
	}
	if (seconds < 0)
	{
		seconds = 0;
	}
	else if (seconds > 59)
	{
		seconds = 59;
	}

	if (snprintf(timeStr, 63, "%02d%02d%02d", hours, minutes, seconds) > 0)
	{
		return timeStr;
	}

	return "";
}

/// Converts from a HHMMSS-formatted string.
time_t TimeConverter::fromHHMMSSString(const string &hhmmss, bool inGMTime)
{
	struct tm timeTm;
	time_t gmTime = 0;

	// Initialize the structure
	timeTm.tm_sec = timeTm.tm_min = timeTm.tm_hour = timeTm.tm_mday = 0;
	timeTm.tm_mon = timeTm.tm_year = timeTm.tm_wday = timeTm.tm_yday = timeTm.tm_isdst = 0;

	strptime(hhmmss.c_str(), "%H%M%S", &timeTm);
#ifdef DEBUG
	cout << "TimeConverter::fromHHMMSSString: " << timeTm.tm_hour << " " << timeTm.tm_min << " " << timeTm.tm_sec << endl;
#endif
	if (inGMTime == true)
	{
		gmTime = timegm(&timeTm);
	}
	else
	{
		gmTime = mktime(&timeTm);
	}

	return gmTime;
}

