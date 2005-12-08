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

#include "Url.h"
#include "IndexedDocument.h"

using std::string;

IndexedDocument::IndexedDocument(const string &title, const string &location,
	const string &originalLocation, const string &type, const string &language) :
	Document(title, location, type, language)
{
	m_originalLocation = originalLocation;
}

IndexedDocument::IndexedDocument(const IndexedDocument &other) :
	Document(other)
{
	m_originalLocation = other.m_originalLocation;
}

IndexedDocument::~IndexedDocument()
{
}

IndexedDocument& IndexedDocument::operator=(const IndexedDocument& other)
{
	m_originalLocation = other.m_originalLocation;
}

/// Returns the document ID.
unsigned int IndexedDocument::getID(void) const
{
	unsigned int docId = 0;

	// Double-check the URL
	Url urlObj(m_location);
	if ((urlObj.getProtocol() == "xapian") ||
		(urlObj.getFile() != ""))
	{
		// The document ID is the final part of the URL, ie the file
		int val = atoi(urlObj.getFile().c_str());
		if (val > 0)
		{
			docId = (unsigned int)val;
		}
	}

	return docId;
}

/// Sets the original location of the document.
void IndexedDocument::setOriginalLocation(const string &originalLocation)
{
	m_originalLocation = originalLocation;
}

/// Returns the original location of the document.
string IndexedDocument::getOriginalLocation(void) const
{
	return m_originalLocation;
}
