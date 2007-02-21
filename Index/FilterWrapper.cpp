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
	return filterDocument(index, doc, 0, labels, docId, false);
}

bool FilterWrapper::updateDocument(unsigned int docId, IndexInterface &index, const Document &doc)
{
	set<string> labels;

	return filterDocument(index, doc, 0, labels, docId, true);
}

bool FilterWrapper::filterDocument(IndexInterface &index, const Document &doc,
	unsigned int count, const set<string> &labels,
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

		// Pass it down to another filter ?
		if (filteredDoc.getType() != "text/plain")
		{
			success = filterDocument(index, filteredDoc, count, labels, docId, doUpdate);
			delete pFilter;

			return success;
		}
		else
		{
			filteredDoc.setType(doc.getType());

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

#ifdef DEBUG
	if (success == false)
	{
		cout << "FilterWrapper::filterDocument: didn't index " << doc.getLocation() << endl;
	}
#endif

	return success;
}
