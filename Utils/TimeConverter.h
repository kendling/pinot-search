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

#ifndef _TIME_CONVERTER_H
#define _TIME_CONVERTER_H

#include <string>

using namespace std;

/// This class handles time conversions.
class TimeConverter
{
	public:
		/// Converts into an RFC 822 timestamp.
		static string toTimestamp(time_t aTime, bool inGMTime = false);

		/// Converts from a RFC 822 timestamp.
		static time_t fromTimestamp(const string &timestamp, bool inGMTime = false);
	
	protected:
		TimeConverter();

	private:
		TimeConverter(const TimeConverter &other);
		TimeConverter& operator=(const TimeConverter& other);

};

#endif // _TIME_CONVERTER_H
