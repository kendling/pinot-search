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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <iostream>
#include <algorithm>

#include "StringManip.h"
#include "Url.h"
#include "FilterFactory.h"
#include "FilterUtils.h"
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
	string location(docInfo.getLocation());
	Url thisUrl(location);
	string protocol(thisUrl.getProtocol());
	string parameters(thisUrl.getParameters());

	if (protocol != "mailbox")
	{
		// We can't handle that type of protocol...
		return NULL;
	}

	// Is there an offset and part number ?
	if ((parameters.find("o=") == string::npos) ||
		(parameters.find("p=") == string::npos))
	{
		return NULL;
	}

	// Get the mbox filter
	Dijon::Filter *pFilter = Dijon::FilterFactory::getFilter("application/mbox");
	if (pFilter == NULL)
	{
		return NULL;
	}

	Document *pMessage = new Document(docInfo);

	location.replace(0, 7, "file");
	string::size_type paramPos = location.find_last_of("?");
	if (paramPos != string::npos)
	{
		location.resize(paramPos);
	}

	pMessage->setLocation(location);
	if (FilterUtils::feedFilter(*pMessage, pFilter) == false)
	{
		delete pFilter;
		return NULL;
	}

	// We want to get extra content like headers
	pFilter->set_property(Dijon::Filter::OPERATING_MODE, "view");

	// The first document should be the message we are interested in
	if ((pFilter->skip_to_document(parameters) == true) &&
		(pFilter->has_documents() == true))
	{
		FilterUtils::populateDocument(*pMessage, pFilter);
	}

	delete pFilter;

	return pMessage;
}
