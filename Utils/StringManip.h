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

#ifndef _STRING_MANIP_H
#define _STRING_MANIP_H

#include <string>

class StringManip
{
	public:
		/// Converts to lowercase.
		static std::string toLowerCase(const std::string &str);

		/// Extracts the sub-string between start and end.
		static std::string extractField(const std::string &str, const std::string &start,
			const std::string &end, bool anyCharacterOfEnd = false);

		/// Extracts the sub-string between start and end.
		static std::string extractField(const std::string &str, const std::string &start,
			const std::string &end, std::string::size_type &endPos, bool anyCharacterOfEnd = false);

		/// Replaces entities.
		static std::string replaceEntities(const std::string &str);

		/// Replaces a sub-string.
		static std::string replaceSubString(const std::string &str, const std::string &substr,
			const std::string &rep);

		/// Removes characters from a string.
		static unsigned int removeCharacters(std::string &str, const std::string &characters);

		/// Removes double and single quotes.
		static std::string removeQuotes(const std::string &str);

		/// Trims spaces at the start and end of a string.
		static unsigned int trimSpaces(std::string &str);

		/// Hashes a string.
		static std::string hashString(const std::string &str);

		/// Hashes a string so that it is no longer than maxLength.
		static std::string hashString(const std::string &str, unsigned int maxLength);

		/// Converts a binary string to an integer.
		static uint32_t binaryStringToInteger(const std::string &str);

		/// Converts an integer to a binary string.
		static std::string integerToBinaryString(uint32_t value);

	protected:
		StringManip();

	private:
		StringManip(const StringManip &other);
		StringManip& operator=(const StringManip& other);

};

#endif // _STRING_MANIP_H
