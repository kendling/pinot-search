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

#include <iostream>
#include <algorithm>

#include "MboxParser.h"
#include "StringManip.h"
#include "Url.h"
#include "MboxCollector.h"

using namespace std;

MboxCollector::MboxCollector() :
	DownloaderInterface()
{
}

MboxCollector::~MboxCollector()
{
}

//
// Implementation of DownloaderInterface
//

/// Retrieves the specified document; NULL if error.
Document *MboxCollector::retrieveUrl(const DocumentInfo &docInfo)
{
	Url thisUrl(docInfo.getLocation());
	string protocol = thisUrl.getProtocol();

	if (protocol != "mailbox")
	{
		// We can't handle that type of protocol...
		return NULL;
	}

	// Extract the offset
	string offset = StringManip::extractField(thisUrl.getParameters(), "o=", "&p=");
	if (offset.empty() == true)
	{
		return NULL;
	}
	off_t messageOffset = (off_t)atol(offset.c_str());

	string directoryName = thisUrl.getLocation();
	string fileName = thisUrl.getFile();
	string fileLocation = directoryName;
	fileLocation += "/";
	fileLocation += fileName;

	// Get a parser
	MboxParser boxParser(fileLocation, messageOffset);
	// The first document should be the message we are interested in
	// FIXME: don't ignore the part number (p=...)
	const Document *pMessage = boxParser.getDocument();
	if (pMessage == NULL)
	{
		return NULL;
	}

	// Copy the message
	Document *fileDocument = new Document(*pMessage);

	return fileDocument;
}
