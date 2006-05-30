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

#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <iostream>

#include "StringManip.h"
#include "XmlTokenizer.h"

//#define DEBUG_TOKENIZER

using std::string;

XmlTokenizer::XmlTokenizer(const Document *pDocument) :
	Tokenizer(NULL)
{
	if (pDocument != NULL)
	{
		unsigned int length = 0;
		const char *data = pDocument->getData(length);

		if ((data != NULL) &&
			(length > 0))
		{
			// Remove XML tags
			string strippedData = parseXML(data);

			// Pass the result to the parent class
			Document *pStrippedDoc = new Document(pDocument->getTitle(),
				pDocument->getLocation(), pDocument->getType(),
				pDocument->getLanguage());
			pStrippedDoc->setData(strippedData.c_str(), strippedData.length());
			setDocument(pStrippedDoc);
		}
	}
}

XmlTokenizer::~XmlTokenizer()
{
	if (m_pDocument != NULL)
	{
		// This should have been set by setDocument(),
		// called in initialize()
		delete m_pDocument;
	}
}

/// Parses XML; the string without tags.
string XmlTokenizer::parseXML(const string &str)
{
	string stripped;
	string::size_type startPos = 0;
	bool isXml = false, skip = false;

	// Tag start
	string::size_type pos = str.find("<");
	while (pos != string::npos)
	{
		isXml = true;

		if (skip == false)
		{
			string text = str.substr(startPos, pos - startPos);

			stripped += StringManip::replaceEntities(text);
			stripped += " ";

			startPos = pos + 1;
			// Tag end
			if (str[pos] == '<')
			{
				pos = str.find(">", startPos);
			}
			// Skip stuff in the tag
			skip = true;
		}
		else
		{
			startPos = pos + 1;
			pos = str.find("<", startPos);
			skip = false;
		}
	}
	if (startPos < str.length())
	{
		stripped  += StringManip::replaceEntities(str.substr(startPos));
	}

	if (isXml == false)
	{
		return str;
	}

	return stripped;
}

/// Utility method that strips XML tags off; the string without tags.
string XmlTokenizer::stripTags(const string &str)
{
	return parseXML(str);
}
