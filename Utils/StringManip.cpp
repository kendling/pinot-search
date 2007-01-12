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
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#include <unistd.h>
#undef _XOPEN_SOURCE
#else
#include <unistd.h>
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#include <ctype.h>
#include <algorithm>

#include "StringManip.h"

using std::string;
using std::for_each;

static const unsigned int HASH_LEN = ((4 * 8 + 5) / 6);

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
string StringManip::extractField(const string &str, const string &start, const string &end, bool anyCharacterOfEnd)
{
	string::size_type endPos = 0;

	return extractField(str, start, end, endPos, anyCharacterOfEnd);
}

/// Extracts the sub-string between start and end.
string StringManip::extractField(const string &str, const string &start, const string &end, string::size_type &endPos, bool anyCharacterOfEnd)
{
	string fieldValue;
	string::size_type startPos = string::npos;

	if (start.empty() == true)
	{
		startPos = 0;
	}
	else
	{
		startPos = str.find(start, endPos);
	}

	if (startPos != string::npos)
	{
		startPos += start.length();

		if (end.empty() == true)
		{
			fieldValue = str.substr(startPos);
		}
		else
		{
			if (anyCharacterOfEnd == false)
			{
				endPos = str.find(end, startPos);
			}
			else
			{
				endPos = str.find_first_of(end, startPos);
			}
			if (endPos != string::npos)
			{
				fieldValue = str.substr(startPos, endPos - startPos);
			}
		}
	}

	return fieldValue;
}

/// Replaces entities.
string StringManip::replaceEntities(const string &str)
{
	// FIXME: replace all
	static const char *escapedChars[] = { "quot", "amp", "lt", "gt", "nbsp", "eacute", "egrave", "agrave", "ccedil"};
	static const char *unescapedChars[] = { "\"", "&", "<", ">", " ", "e", "e", "a", "c"};
	static const unsigned int escapedCharsCount = 9;
	string unescapedStr;
	string::size_type startPos = 0;

	string::size_type pos = str.find("&");
	while (pos != string::npos)
	{
		unescapedStr += str.substr(startPos, pos - startPos);

		startPos = pos + 1;
		pos = str.find(";", startPos);
		if ((pos != string::npos) &&
			(pos < startPos + 10))
		{
			string escapedChar = str.substr(startPos, pos - startPos);
			bool replacedChar = false;

			// See if we can replace this with an actual character
			for (unsigned int count = 0; count < escapedCharsCount; ++count)
			{
				if (escapedChar == escapedChars[count])
				{
					unescapedStr += unescapedChars[count];
					replacedChar = true;
					break;
				}
			}

			if (replacedChar == false)
			{
				// This couldn't be replaced, leave it as it is...
				unescapedStr += "&";
				unescapedStr += escapedChar;
				unescapedStr += ";";
			}

			startPos = pos + 1;
		}

		// Next
		pos = str.find("&", startPos);
	}
	if (startPos < str.length())
	{
		unescapedStr  += str.substr(startPos);
	}

	return unescapedStr;
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
		++count;

		charPos = str.find_first_of(characters.c_str(), charPos);
	}

	return count;	
}

/// Trims spaces at the start and end of a string.
unsigned int StringManip::trimSpaces(string &str)
{
	unsigned int pos = 0;
	unsigned int count = 0;

	while ((str.empty() == false) && (pos < str.length()))
	{
		if (isspace(str[pos]) == 0)
		{
			++pos;
			break;
		}

		str.erase(pos, 1);
		++count;
	}

	for (unsigned int pos = str.length() - 1;
		(str.empty() == false) && (pos >= 0); --pos)
	{
		if (isspace(str[pos]) == 0)
		{
			break;
		}

		str.erase(pos, 1);
		++count;
	}

	return count;
}

/// Removes double and single quotes in links or any other attribute.
string StringManip::removeQuotes(const string &str)
{
	string unquotedText;

	if (str[0] == '"')
	{
		string::size_type closingQuotePos = str.find("\"", 1);
		if (closingQuotePos != string::npos)
		{
			unquotedText = str.substr(1, closingQuotePos - 1);
		}
	}
	else if (str[0] == '\'')
	{
		string::size_type closingQuotePos = str.find("'", 1);
		if (closingQuotePos != string::npos)
		{
			unquotedText = str.substr(1, closingQuotePos - 1);
		}
	}
	else
	{
		// There are no quotes, so just look for the first space, if any
		string::size_type spacePos = str.find(" ");
		if (spacePos != string::npos)
		{
			unquotedText = str.substr(0, spacePos);
		}
		else
		{
			unquotedText = str;
		}
	}

	return unquotedText;
}

/// Hashes a string.
string StringManip::hashString(const string &str)
{
	unsigned long int h = 1;

	if (str.empty() == true)
	{
		return "";
	}

	// The following was lifted from Xapian's xapian-applications/omega/hashterm.cc
	// and prettified slightly
	for (string::const_iterator i = str.begin(); i != str.end(); ++i)
	{
		h += (h << 5) + static_cast<unsigned char>(*i);
	}
	// In case sizeof(unsigned long) > 4
	h &= 0xffffffff;

	string hashedString(HASH_LEN, ' ');
	int i = 0;
	while (h != 0)
	{
		char ch = char((h & 63) + 33);
		hashedString[i++] = ch;
		h = h >> 6;
	}

	return hashedString;
}

/// Hashes a string so that it is no longer than maxLength.
string StringManip::hashString(const string &str, unsigned int maxLength)
{
	if (str.length() <= maxLength)
	{
		return str;
	}

	string result(str);
	maxLength -= HASH_LEN;
	result.replace(maxLength, string::npos,
		hashString(result.substr(maxLength)));

	return result;
}

/// Converts a binary string to an integer.
uint32_t StringManip::binaryStringToInteger(const string &str)
{
	uint32_t value;

	// This is from from Xapian's xapian-applications/omega/values.h
	if (str.size() != 4)
	{
		return (uint32_t)-1;
	}

	memcpy(&value, str.data(), 4);

	return ntohl(value);
}

/// Converts an integer to a binary string.
string StringManip::integerToBinaryString(uint32_t value)
{
	// This is from from Xapian's xapian-applications/omega/values.h
	value = htonl(value);

	return string(reinterpret_cast<const char*>(&value), 4);
}

