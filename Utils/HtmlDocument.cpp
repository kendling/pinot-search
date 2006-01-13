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

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>
#include <ctype.h>
#include <iostream>
#include <utility>

#include "HtmlDocument.h"
#include "StringManip.h"

using std::cout;
using std::endl;
using std::string;
using std::min;

HtmlDocument::HtmlDocument(const string &title, const string &location,
	const string &type, const string &language) :
	Document(title, location, type, language)
{
}

HtmlDocument::HtmlDocument(const HtmlDocument &other) :
	Document(other)
{
}

HtmlDocument::~HtmlDocument()
{
}

void HtmlDocument::parseHead(void)
{
	char *pBodyStart = strstr(m_pData, "<body");
	if (pBodyStart == NULL)
	{
		pBodyStart = strstr(m_pData, "<BODY");
	}
	if (pBodyStart != NULL)
	{
		string htmlHead(m_pData, pBodyStart - m_pData);
		regex_t titleRegex, httpEquivRegex;
		regmatch_t pTitleMatches[3];
		regmatch_t pHttpEquivMatches[3];
		int titleMatches = 3, httpEquivMatches = 3;

		// Look for a title
		if (regcomp(&titleRegex, "<title([^>]*)>([^<>]*)</title", REG_EXTENDED|REG_ICASE) == 0)
		{
			if ((regexec(&titleRegex, htmlHead.c_str(), titleMatches,
					pTitleMatches, REG_NOTBOL|REG_NOTEOL) == 0) &&
				(pTitleMatches[titleMatches - 1].rm_so != -1))
			{
				string title = htmlHead.substr(pTitleMatches[2].rm_so,
					pTitleMatches[2].rm_eo - pTitleMatches[2].rm_so);

				if (title.empty() == false)
				{
					// Override the title
					m_title = title;
				}
			}
		}
		// ...and a Content-Type
		if (regcomp(&httpEquivRegex, "<meta http-equiv=([^>]*) content=([^>]*)>", REG_EXTENDED|REG_ICASE) == 0)
		{
			if ((regexec(&httpEquivRegex, htmlHead.c_str(), httpEquivMatches,
					pHttpEquivMatches, REG_NOTBOL|REG_NOTEOL) == 0) &&
				(pHttpEquivMatches[httpEquivMatches - 1].rm_so != -1))
			{
				string name = StringManip::removeQuotes(
					htmlHead.substr(pHttpEquivMatches[1].rm_so,
					pHttpEquivMatches[1].rm_eo - pHttpEquivMatches[1].rm_so));
				string content = StringManip::removeQuotes(
					htmlHead.substr(pHttpEquivMatches[2].rm_so,
					pHttpEquivMatches[2].rm_eo - pHttpEquivMatches[2].rm_so));

				if ((content.empty() == false) &&
					(strncasecmp(name.c_str(), "Content-Type",
						min((int)name.length(), 12)) == 0))
				{
					// Override the type
					m_type = content;
				}
			}
		}
#ifdef DEBUG
		cout << "HtmlDocument::parseHead: extracted title " << m_title <<
			", type " << m_type << endl;
#endif

		regfree(&titleRegex);
		regfree(&httpEquivRegex);
	}
}

/// Copies the given data in the document.
bool HtmlDocument::setData(const char *data, unsigned int length)
{
	if ((data == NULL) ||
		(length == 0))
	{
		return false;
	}

	// Discard existing data
	freeData();

	// FIXME: there must be a way of getting rid of this
	m_pData = (char *)calloc(length + 1, sizeof(char));
	if (m_pData != NULL)
	{
		// Removing non-printable characters, as found in pages from alltheweb sometimes
		// They prevent us from using strstr() and mess up the regexps
		for (unsigned int i = 0; i < length; ++i)
		{
			if (isprint(data[i]) == 0)
			{
				m_pData[i] = ' ';
			}
			else
			{
				m_pData[i] = data[i];
			}
		}
		m_dataLength = length;

		parseHead();

		return true;
	}

	return false;
}

/// Maps the given file.
bool HtmlDocument::setDataFromFile(const string &fileName)
{
	if (Document::setDataFromFile(fileName) == true)
	{
		parseHead();

		return true;
	}

	return false;
}
