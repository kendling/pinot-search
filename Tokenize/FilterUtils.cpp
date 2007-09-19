/*
 *  Copyright 2007 Fabrice Colin
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

#include <stdlib.h>
#include <unistd.h> 
#include <iostream>
#include <set>

#include "MIMEScanner.h"
#include "StringManip.h"
#include "Url.h"
#include "FilterFactory.h"
#include "FilterUtils.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::set;
using std::map;

map<string, string> FilterUtils::m_typeAliases;

FilterUtils::FilterUtils()
{
}

FilterUtils::~FilterUtils()
{
}

Dijon::Filter *FilterUtils::getFilter(const string &mimeType)
{
	Dijon::Filter *pFilter = NULL;

	// Is this type aliased ?
	map<string, string>::const_iterator aliasIter = m_typeAliases.find(mimeType);
	if (aliasIter != m_typeAliases.end())
	{
		pFilter = Dijon::FilterFactory::getFilter(aliasIter->second);
	}
	else
	{
		// Is there a filter for this type ?
		pFilter = Dijon::FilterFactory::getFilter(mimeType);
	}

	if (pFilter != NULL)
	{
		return pFilter;
	}

	if (mimeType.empty() == false)
	{
		set<string> parentTypes;

		// Try that type's parents
		MIMEScanner::getParentTypes(mimeType, parentTypes);
		for (set<string>::const_iterator parentIter = parentTypes.begin();
			parentIter != parentTypes.end(); ++parentIter)
		{
			pFilter = Dijon::FilterFactory::getFilter(*parentIter);
			if (pFilter != NULL)
			{
				// Add an alias
				m_typeAliases[mimeType] = *parentIter;
				return pFilter;
			}
		}
	}

	return NULL;
}

bool FilterUtils::isSupportedType(const string &mimeType)
{
	// Is this type aliased ?
	map<string, string>::const_iterator aliasIter = m_typeAliases.find(mimeType);
	if (aliasIter != m_typeAliases.end())
	{
		// There's an alias because we were able to get a filter for this parent type
		// or a previous call to isSupportedType() succeeded
		return true;
	}

	if (Dijon::FilterFactory::isSupportedType(mimeType) == false)
	{
		set<string> parentTypes;

		// Try that type's parents
		MIMEScanner::getParentTypes(mimeType, parentTypes);
		for (set<string>::const_iterator parentIter = parentTypes.begin();
			parentIter != parentTypes.end(); ++parentIter)
		{
			if (Dijon::FilterFactory::isSupportedType(*parentIter) == true)
			{
				// Add an alias
				m_typeAliases[mimeType] = *parentIter;
				return true;
			}
		}
	}

	return false;
}

bool FilterUtils::feedFilter(const Document &doc, Dijon::Filter *pFilter)
{
	string location(doc.getLocation());
	Url urlObj(location);
	string fileName;
	unsigned int dataLength = 0;
	const char *pData = doc.getData(dataLength);
	bool fedInput = false;

	if (pFilter == NULL)
	{
		return false;
	}

	if ((urlObj.getProtocol() == "file") &&
		(location .length() > 7))
	{
		fileName = location.substr(7);
	}

	// Prefer feeding the data
	if (((dataLength > 0) && (pData != NULL)) &&
		(pFilter->is_data_input_ok(Dijon::Filter::DOCUMENT_DATA) == true))
	{
#ifdef DEBUG
		cout << "FilterUtils::feedFilter: feeding " << dataLength << " bytes of data from " << location << endl;
#endif
		fedInput = pFilter->set_document_data(pData, dataLength);
	}
	// ... to feeding the file
	if ((fedInput == false) &&
		(fileName.empty() == false))
	{ 
		if (pFilter->is_data_input_ok(Dijon::Filter::DOCUMENT_FILE_NAME) == true)
		{
#ifdef DEBUG
			cout << "FilterUtils::feedFilter: feeding file " << fileName << endl;
#endif
			fedInput = pFilter->set_document_file(fileName);
		}
		// ...and to feeding the file's contents
		if ((fedInput == false) &&
			(pFilter->is_data_input_ok(Dijon::Filter::DOCUMENT_DATA) == true))
		{
			Document docCopy(doc);

			if (docCopy.setDataFromFile(fileName) == false)
			{
				cerr << "Couldn't load " << fileName << endl;

				return false;
			}
#ifdef DEBUG
			cout << "FilterUtils::feedFilter: feeding contents of file " << fileName << endl;
#endif

			pData = docCopy.getData(dataLength);
			if ((dataLength > 0) &&
				(pData != NULL))
			{
				fedInput = pFilter->set_document_data(pData, dataLength);
			}
			else
			{
				// The file may be empty
				fedInput = pFilter->set_document_data(" ", 1);
			}
		}
	}
	// ... to feeding data through a temporary file
	if ((fedInput == false) &&
		((dataLength > 0) && (pData != NULL)) &&
		(pFilter->is_data_input_ok(Dijon::Filter::DOCUMENT_FILE_NAME) == true))
	{
		char inTemplate[18] = "/tmp/filterXXXXXX";

		int inFd = mkstemp(inTemplate);
		if (inFd != -1)
		{
#ifdef DEBUG
			cout << "FilterUtils::feedFilter: feeding temporary file " << inTemplate << endl;
#endif

			// Save the data
			if (write(inFd, (const void*)pData, dataLength) != -1)
			{
				fedInput = pFilter->set_document_file(inTemplate, true);
				if (fedInput == false)
				{
					// We might as well delete the file now
					unlink(inTemplate);
				}
			}

			close(inFd);
		}
	}

	if (fedInput == false)
	{
		cerr << "Couldn't feed filter for " << doc.getLocation() << endl;

		return false;
	}

	return true;
}

bool FilterUtils::populateDocument(Document &doc, Dijon::Filter *pFilter)
{
	string uri, ipath;

	if (pFilter == NULL)
	{
		return false;
	}

	const map<string, string> &metaData = pFilter->get_meta_data();
	for (map<string, string>::const_iterator metaIter = metaData.begin();
		metaIter != metaData.end(); ++metaIter)
	{
		if (metaIter->first == "content")
		{
			doc.setData(metaIter->second.c_str(), metaIter->second.length());
#ifdef DEBUG
			cout << "FilterUtils::populateDocument: set " << metaIter->second.length()
				<< " characters of data" << endl;
#endif
		}
		else if (metaIter->first == "ipath")
		{
			ipath = metaIter->second;
		}
		else if (metaIter->first == "language")
		{
			doc.setLanguage(metaIter->second);
		}
		else if (metaIter->first == "mimetype")
		{
			doc.setType(StringManip::toLowerCase(metaIter->second));
		}
		else if (metaIter->first == "size")
		{
			doc.setSize((off_t)atoi(metaIter->second.c_str()));
		}
		else if (metaIter->first == "title")
		{
			doc.setTitle(metaIter->second);
		}
		else if (metaIter->first == "uri")
		{
			uri = metaIter->second;

			if ((uri.length() >= 18) &&
				(uri.substr(0, 18) == "file:///tmp/filter"))
			{
				// We fed the filter a temporary file
				uri.clear();
			}
		}
	}

	if (uri.empty() == false)
	{
		doc.setLocation(uri);
	}
	if (ipath.empty() == false)
	{
		doc.setLocation(doc.getLocation() + "?" + ipath);
	}

	return true;
}

string FilterUtils::stripMarkup(const string &text)
{
	if (text.empty() == true)
	{
		return "";
	}

	Dijon::Filter *pFilter = Dijon::FilterFactory::getFilter("text/xml");

	if (pFilter == NULL)
	{
		return "";
	}

	Document doc;
	string strippedText;

	doc.setData(text.c_str(), text.length());
	if ((feedFilter(doc, pFilter) == true) &&
		(pFilter->next_document() == true))
	{
		const map<string, string> &metaData = pFilter->get_meta_data();
		map<string, string>::const_iterator contentIter = metaData.find("content");
		if (contentIter != metaData.end())
		{
			strippedText = contentIter->second;
		}
	}

	delete pFilter;

	return strippedText;
}
