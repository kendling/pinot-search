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

#include <iostream>
#include <map>

#include "Url.h"
#include "FilterFactory.h"
#include "FilterUtils.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::set;
using std::map;
using namespace Dijon;

FilterUtils::FilterUtils()
{
}

FilterUtils::~FilterUtils()
{
}

bool FilterUtils::feedFilter(const Document &doc, Dijon::Filter *pFilter)
{
	string location(doc.getLocation());
	Url urlObj(location);
	string fileName;
	unsigned int dataLength = 0;
	const char *pData = doc.getData(dataLength);
	bool fedInput = false, success = false;

	if (pFilter == NULL)
	{
		return false;
	}

	if ((urlObj.getProtocol() == "file") &&
		(location .length() > 7))
	{
		fileName = location.substr(7);
	}
	else if ((urlObj.getProtocol() == "mailbox") &&
		(location .length() > 10))
	{
		fileName = location.substr(10);
		string::size_type pos = fileName.find("?");
		if (pos != string::npos)
		{
			fileName.resize(pos);
		}
	}

	// Prefer feeding the data
	if (((dataLength > 0) && (pData != NULL)) &&
		(pFilter->is_data_input_ok(Filter::DOCUMENT_DATA) == true))
	{
#ifdef DEBUG
		cout << "FilterUtils::getFilter: feeding data" << endl;
#endif
		fedInput = pFilter->set_document_data(pData, dataLength);
	}
	// ... to feeding the file
	if ((fedInput == false) &&
		(fileName.empty() == false))
	{ 
		if (pFilter->is_data_input_ok(Filter::DOCUMENT_FILE_NAME) == true)
		{
#ifdef DEBUG
			cout << "FilterUtils::getFilter: feeding file " << fileName << endl;
#endif
			fedInput = pFilter->set_document_file(fileName);
		}
		// ...and to feeding the file's contents
		if ((fedInput == false) &&
			(pFilter->is_data_input_ok(Filter::DOCUMENT_DATA) == true))
		{
			Document docCopy(doc);

			if (docCopy.setDataFromFile(fileName) == false)
			{
				cerr << "Couldn't load " << fileName << endl;

				return false;
			}
#ifdef DEBUG
			cout << "FilterUtils::getFilter: feeding contents of file " << fileName << endl;
#endif

			pData = docCopy.getData(dataLength);
			if ((dataLength > 0) &&
				(pData != NULL))
			{
				fedInput = pFilter->set_document_data(pData, dataLength);
			}
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
			cout << "FilterUtils:populateDocument: set " << metaIter->second.length()
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
			doc.setType(metaIter->second);
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
	Dijon::Filter *pFilter = Dijon::FilterFactory::getFilter("text/xml");
	string strippedText;

	if (pFilter == NULL)
	{
		return "";
	}

	Document doc;
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
