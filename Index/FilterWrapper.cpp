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

#include "Url.h"
#include "FilterFactory.h"
#include "TextFilter.h"
#include "FilterUtils.h"
#include "FilterWrapper.h"

using std::cout;
using std::endl;
using std::string;
using std::set;
using namespace Dijon;

FilterWrapper::FilterWrapper()
{
}

FilterWrapper::~FilterWrapper()
{
}

bool FilterWrapper::indexDocument(IndexInterface &index, const Document &doc,
	const set<string> &labels, unsigned int &docId)
{
	string originalType(doc.getType());

	return filterDocument(index, doc, originalType, labels, docId, false);
}

bool FilterWrapper::updateDocument(unsigned int docId, IndexInterface &index, const Document &doc)
{
	set<string> labels;
	string originalType(doc.getType());

	return filterDocument(index, doc, originalType, labels, docId, true);
}

bool FilterWrapper::filterDocument(IndexInterface &index, const Document &doc,
	const string &originalType, const set<string> &labels,
	unsigned int &docId, bool doUpdate)
{
	Filter *pFilter = FilterFactory::getFilter(doc.getType());
	bool fedFilter = false, success = false;

	if (pFilter != NULL)
	{
		fedFilter = FilterUtils::feedFilter(doc, pFilter);
	}
	else
	{
		// Chances are this type is not supported
		pFilter = new TextFilter("text/plain");

		Document emptyDoc(doc.getTitle(), doc.getLocation(), doc.getType(), doc.getLanguage());

		emptyDoc.setTimestamp(doc.getTimestamp());
		emptyDoc.setSize(doc.getSize());
		emptyDoc.setData(" ", 1);

#ifdef DEBUG
		cout << "FilterWrapper::filterDocument: unsupported type " << doc.getType() << endl;
#endif
		fedFilter = FilterUtils::feedFilter(emptyDoc, pFilter);
	}

	if (fedFilter == false)
	{
		delete pFilter;

		return false;
	}

	while (pFilter->has_documents() == true)
	{
		string actualType(originalType);

		if (pFilter->next_document() == false)
		{
			break;
		}

		Document filteredDoc(doc.getTitle(), doc.getLocation(), "text/plain", doc.getLanguage());

		filteredDoc.setTimestamp(doc.getTimestamp());
		filteredDoc.setSize(doc.getSize());

		if (FilterUtils::populateDocument(filteredDoc, pFilter) == false)
		{
			continue;
		}

		// Is this a nested document ?
		if (filteredDoc.getLocation().length() > doc.getLocation().length())
		{
			actualType = filteredDoc.getType();
#ifdef DEBUG
			cout << "FilterWrapper::filterDocument: nested document of type " << actualType << endl;
#endif
		}

		// Pass it down to another filter ?
		if ((filteredDoc.getType().length() >= 10) &&
			(filteredDoc.getType().substr(0, 10) == "text/plain"))
		{
			// No, it's been reduced to plain text
			filteredDoc.setType(actualType);

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
		else
		{
			success = filterDocument(index, filteredDoc, originalType, labels, docId, doUpdate);
			delete pFilter;

			return success;
		}
	}

	delete pFilter;

#ifdef DEBUG
	if (success == false)
	{
		cout << "FilterWrapper::filterDocument: didn't index " << doc.getLocation() << endl;
	}
#endif

	return success;
}
