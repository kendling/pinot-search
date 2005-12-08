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

#include <ctype.h>
#include <stdlib.h>

#include "HtmlDocument.h"

using std::string;

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
	}

	return true;
}
