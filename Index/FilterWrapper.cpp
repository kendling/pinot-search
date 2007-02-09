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
#include "FilterWrapper.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::set;
using std::map;
using namespace Dijon;

FilterWrapper::FilterWrapper()
{
}

FilterWrapper::~FilterWrapper()
{
}

bool FilterWrapper::feedFilter(const Document &doc, Dijon::Filter *pFilter)
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

	// Prefer feeding the data
	if (((dataLength > 0) && (pData != NULL)) &&
		(pFilter->is_data_input_ok(Filter::DOCUMENT_DATA) == true))
	{
#ifdef DEBUG
		cout << "FilterWrapper::getFilter: feeding data" << endl;
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
			cout << "FilterWrapper::getFilter: feeding file " << fileName << endl;
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
			cout << "FilterWrapper::getFilter: feeding contents of file " << fileName << endl;
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

string FilterWrapper::stripMarkup(const string &text)
{
	Dijon::Filter *pFilter = Dijon::FilterFactory::getFilter("text/xml");
	string strippedText;

	if (pFilter == NULL)
	{
		return "";
	}

	Document doc;
	doc.setData(text.c_str(), text.length());
	if (feedFilter(doc, pFilter) == true)
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

bool FilterWrapper::indexDocument(IndexInterface &index, const Document &doc,
	const set<string> &labels, unsigned int &docId)
{
	Document docCopy(doc);

	return filterDocument(index, docCopy, 0, labels, docId, false);
}

/// Updates the given document.
bool FilterWrapper::updateDocument(unsigned int docId, IndexInterface &index, const Document &doc)
{
	Document docCopy(doc);
	set<string> labels;

	return filterDocument(index, docCopy, 0, labels, docId, true);
}

bool FilterWrapper::filterDocument(IndexInterface &index, Document &doc,
	unsigned int count, const set<string> &labels, unsigned int &docId,
	bool doUpdate)
{
	Filter *pFilter = FilterFactory::getFilter(doc.getType());
	bool success = false;

	if (pFilter == NULL)
	{
		return false;
	}

	if (feedFilter(doc, pFilter) == false)
	{
		delete pFilter;

		return false;
	}

	while (pFilter->has_documents() == true)
	{
		Document filteredDoc(doc.getTitle(), doc.getLocation(), "text/plain", doc.getLanguage());
		string uri, ipath;
		unsigned int dataLength = 0;

		if (pFilter->next_document() == false)
		{
			break;
		}
		cout << "##### Document " << ++count << endl;

		const map<string, string> &metaData = pFilter->get_meta_data();
		for (map<string, string>::const_iterator metaIter = metaData.begin();
			metaIter != metaData.end(); ++metaIter)
		{
			if (metaIter->first == "content")
			{
				filteredDoc.setData(metaIter->second.c_str(), metaIter->second.length());

				cout << "Set " << metaIter->second.length() << " characters of data" << endl;
			}
			else if (metaIter->first == "ipath")
			{
				ipath = metaIter->second;
			}
			else if (metaIter->first == "language")
			{
				filteredDoc.setLanguage(metaIter->second);
			}
			else if (metaIter->first == "mimetype")
			{
				filteredDoc.setType(metaIter->second);
			}
			else if (metaIter->first == "size")
			{
				filteredDoc.setSize((off_t)atoi(metaIter->second.c_str()));
			}
			else if (metaIter->first == "title")
			{
				filteredDoc.setTitle(metaIter->second);
			}
			else if (metaIter->first == "uri")
			{
				uri = metaIter->second;
			}
		}

		if (uri.empty() == false)
		{
			filteredDoc.setLocation(uri);
		}
		if (ipath.empty() == false)
		{
			filteredDoc.setLocation(filteredDoc.getLocation() + "?" + ipath);
		}
		cout << "Location: " << filteredDoc.getLocation() << endl;
		cout << "MIME type: " << filteredDoc.getType() << endl;

		// Pass it down to another filter ?
		if (filteredDoc.getType() != "text/plain")
		{
			cout << "Passing down to another filter" << endl;

			filterDocument(index, filteredDoc, count, labels, docId, doUpdate);
		}
		else
		{
			Tokenizer tokens(&filteredDoc);

			if (doUpdate == false)
			{
				success = index.indexDocument(tokens, labels, docId);
			}
			else
			{
				success = index.updateDocument(docId, tokens);
			}
		}
	}

	delete pFilter;

	return success;
}

