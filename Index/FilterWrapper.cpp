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
#include "FilterUtils.h"
#include "FilterWrapper.h"

using std::cout;
using std::cerr;
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
	Document docCopy(doc);

	return filterDocument(index, docCopy, 0, labels, docId, false);
}

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

	if (FilterUtils::feedFilter(doc, pFilter) == false)
	{
		delete pFilter;

		return false;
	}

	while (pFilter->has_documents() == true)
	{
		Document filteredDoc(doc.getTitle(), doc.getLocation(), "text/plain", doc.getLanguage());

		if (pFilter->next_document() == false)
		{
			break;
		}

		if (FilterUtils::populateDocument(filteredDoc, pFilter) == false)
		{
			continue;
		}

		// Pass it down to another filter ?
		if (filteredDoc.getType() != "text/plain")
		{
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
