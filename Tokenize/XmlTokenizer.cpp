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

#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <iostream>

#include "StringManip.h"
#include "XmlTokenizer.h"

//#define DEBUG_TOKENIZER

using std::string;

XmlTokenizer::XmlTokenizer(const Document *pDocument) :
	Tokenizer(NULL),
	m_pStrippedDocument(NULL)
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
			m_pStrippedDocument = new Document(pDocument->getTitle(),
				pDocument->getLocation(), pDocument->getType(),
				pDocument->getLanguage());
			m_pStrippedDocument->setData(strippedData.c_str(), strippedData.length());
			m_pStrippedDocument->setTimestamp(pDocument->getTimestamp());
			m_pStrippedDocument->setSize(pDocument->getSize());

			setDocument(m_pStrippedDocument);
		}
	}
}

XmlTokenizer::~XmlTokenizer()
{
	if (m_pStrippedDocument != NULL)
	{
		delete m_pStrippedDocument;
	}
}

/// Parses XML; the string without tags.
string XmlTokenizer::parseXML(const string &str)
{
	if (str.empty() == true)
	{
		return "";
	}

	string stripped(StringManip::replaceEntities(str));

	// Tag start
	string::size_type startPos = stripped.find("<");
	while (startPos != string::npos)
	{
		string::size_type endPos = stripped.find(">", startPos);
		if (endPos == string::npos)
		{
			break;
		}

		stripped.erase(startPos, endPos - startPos + 1);

		// Next
		startPos = stripped.find("<");
	}

	// The input may contain partial tags, eg "a>...</a><b>...</b>...<c"
	string::size_type pos = stripped.find(">");
	if (pos != string::npos)
	{
		stripped.erase(0, pos + 1);
	}
	pos = stripped.find("<");
	if (pos != string::npos)
	{
		stripped.erase(pos);
	}

	return stripped;
}

/// Utility method that strips XML tags off; the string without tags.
string XmlTokenizer::stripTags(const string &str)
{
	return parseXML(str);
}
