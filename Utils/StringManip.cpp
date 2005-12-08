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

#define _XOPEN_SOURCE
#include <unistd.h>
#undef _XOPEN_SOURCE
#include <ctype.h>
#include <algorithm>

#include "StringManip.h"

using std::string;
using std::for_each;

#define CRYPT_SALT "$1$ajfpehom$"

// A function object to lower case strings with for_each()
struct ToLower
{
	public:
		void operator()(char &c)
		{
			c = (char)tolower((int)c);
		}
};

StringManip::StringManip()
{
}

/// Converts to lowercase.
string StringManip::toLowerCase(const string &str)
{
	string tmp = str;

	for_each(tmp.begin(), tmp.end(), ToLower());

	return tmp;
}

/// Extracts the sub-string between start and end.
string StringManip::extractField(const string &str, const string &start, const string &end)
{
	string::size_type endPos = 0;

	return extractField(str, start, end, endPos);
}

/// Extracts the sub-string between start and end.
string StringManip::extractField(const string &str, const string &start, const string &end, string::size_type &endPos)
{
	string fieldValue;

	string::size_type startPos = str.find(start, endPos);
	if (startPos != string::npos)
	{
		startPos += start.length();
		endPos = str.find(end, startPos);
		if (endPos != string::npos)
		{
			fieldValue = str.substr(startPos, endPos - startPos);
		}
	}

	return fieldValue;
}

/// Replaces a sub-string.
string StringManip::replaceSubString(const string &str, const std::string &substr, const std::string &rep)
{
	string cleanStr = str;

	string::size_type startPos = cleanStr.find(substr);
	while (startPos != string::npos)
	{
		string::size_type endPos = startPos + substr.length();

		string tmp = cleanStr.substr(0, startPos);
		tmp += rep;
		tmp += cleanStr.substr(endPos);
		cleanStr = tmp;

		startPos += rep.length();
		if (startPos > cleanStr.length())
		{
			break;
		}

		startPos = cleanStr.find(substr, startPos);
	}

	return cleanStr;
}

/// Removes characters from a string.
unsigned int StringManip::removeCharacters(string &str, const string &characters)
{
	unsigned int count = 0;

	string::size_type charPos = str.find_first_of(characters.c_str());
	while (charPos != string::npos)
	{
		str.erase(charPos, 1);
		charPos = str.find_first_of(characters.c_str(), charPos - 1);
	}

	return count;	
}

/// Hashes a string.
string StringManip::hashString(string &str)
{
	if (str.empty() == true)
	{
		return "";
	}

	char *hashedString = crypt(str.c_str(), CRYPT_SALT);
	if (hashedString == NULL)
	{
		return NULL;
	}

	if (strlen(hashedString) > strlen(CRYPT_SALT))
	{
		if (strncmp(hashedString, CRYPT_SALT, strlen(CRYPT_SALT)) == 0)
		{
			return hashedString + strlen(CRYPT_SALT);
		}
	}

	return hashedString;
}
