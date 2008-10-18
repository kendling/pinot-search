/*
 *  Copyright 2007-2008 Fabrice Colin
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

#include <time.h>
#include <iostream>

#include "Url.h"
#include "FilterFactory.h"
#include "TextFilter.h"
#include "FilterUtils.h"
#include "TextConverter.h"
#include "FilterWrapper.h"

using std::cout;
using std::endl;
using std::string;
using std::set;
using namespace Dijon;

static string convertToUTF8(const char *pData, unsigned int dataLen, const string &charset)
{
	TextConverter converter(20);

#ifdef DEBUG
	cout << "FilterWrapper::filterDocument: filter requested conversion from " << charset << endl;
#endif
	return converter.toUTF8(pData, dataLen, charset);
}

ReducedAction::ReducedAction()
{
}

ReducedAction::~ReducedAction()
{
}

class IndexAction : public ReducedAction
{
	public:
		IndexAction(IndexInterface *pIndex, const set<string> &labels,
			unsigned int docId, bool doUpdate) :
			ReducedAction(),
			m_pIndex(pIndex),
			m_labels(labels),
			m_docId(docId),
			m_doUpdate(doUpdate)
		{
		}

		virtual ~IndexAction()
		{
		}

		virtual bool takeAction(Document &doc, bool isNested)
		{
			bool docSuccess = false;

			// Nested documents can't be updated because they are unindexed
			// and the ID is that of the base document anyway
			if ((m_doUpdate == true) &&
				(isNested == false))
			{
				docSuccess = m_pIndex->updateDocument(m_docId, doc);
			}
			else
			{
				unsigned int newDocId = m_docId;

				docSuccess = m_pIndex->indexDocument(doc, m_labels, newDocId);
				// Make sure we return the base document's ID, not the last nested document's ID
				if (isNested == false)
				{
					m_docId = newDocId;
				}
			}

			return docSuccess;
		}

		unsigned int getId(void) const
		{
			return m_docId;
		}

	public:
		IndexInterface *m_pIndex;
		const set<string> &m_labels;
		unsigned int m_docId;
		bool m_doUpdate;

	private:
		IndexAction(const IndexAction &other);
		IndexAction &operator=(const IndexAction &other);

};

FilterWrapper::FilterWrapper(IndexInterface *pIndex) :
	m_pIndex(pIndex)
{
}

FilterWrapper::~FilterWrapper()
{
}

bool FilterWrapper::reduceToText(const Document &doc, ReducedAction &action)
{
	string originalType(doc.getType());
	unsigned int indexId = 0;

	return filterDocument(doc, originalType, action);
}

bool FilterWrapper::indexDocument(const Document &doc, const set<string> &labels, unsigned int &docId)
{
	string originalType(doc.getType());

	if (m_pIndex == NULL)
	{
		return false;
	}

	unindexNestedDocuments(doc.getLocation());

	IndexAction action(m_pIndex, labels, docId, false);

	bool filteredDoc = filterDocument(doc, originalType, action);
	docId = action.getId();

	return filteredDoc;
}

bool FilterWrapper::updateDocument(const Document &doc, unsigned int docId)
{
	set<string> labels;
	string originalType(doc.getType());

	if (m_pIndex == NULL)
	{
		return false;
	}

	unindexNestedDocuments(doc.getLocation());

	IndexAction action(m_pIndex, labels, docId, true);

	return filterDocument(doc, originalType, action);
}

bool FilterWrapper::unindexDocument(const string &location)
{
	if (m_pIndex == NULL)
	{
		return false;
	}

	unindexNestedDocuments(location);

	return m_pIndex->unindexDocument(location);
}

bool FilterWrapper::filterDocument(const Document &doc, const string &originalType,
	ReducedAction &action)
{
	Filter *pFilter = FilterUtils::getFilter(doc.getType());
	bool fedFilter = false, docSuccess = false, finalSuccess = false;

	if (pFilter != NULL)
	{
		// The filter may have to convert the content to UTF-8 itself
    		pFilter->set_utf8_converter(convertToUTF8);

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

	bool hasDocs = pFilter->has_documents();
#ifdef DEBUG
	cout << "FilterWrapper::filterDocument: has documents " << hasDocs << endl;
#endif
	while (hasDocs == true)
	{
		string actualType(originalType);
		bool isNested = false;
		bool emptyTitle = false;

		if (pFilter->next_document() == false)
		{
#ifdef DEBUG
			cout << "FilterWrapper::filterDocument: no more documents in " << doc.getLocation() << endl;
#endif
			break;
		}

		string originalTitle(doc.getTitle());
		Document filteredDoc(originalTitle, doc.getLocation(), "text/plain", doc.getLanguage());

		filteredDoc.setTimestamp(doc.getTimestamp());
		filteredDoc.setSize(doc.getSize());
		docSuccess = false;

		if (FilterUtils::populateDocument(filteredDoc, pFilter) == false)
		{
			hasDocs = pFilter->has_documents();
			continue;
		}

		// Is this a nested document ?
		if (filteredDoc.getLocation().length() > doc.getLocation().length())
		{
			actualType = filteredDoc.getType();
#ifdef DEBUG
			cout << "FilterWrapper::filterDocument: nested document of type " << actualType << endl;
#endif
			isNested = true;
		}
		else if (originalTitle.empty() == false)
		{
			// Preserve the top-level document's title
			filteredDoc.setTitle(originalTitle);
		}
		else if (filteredDoc.getTitle().empty() == true)
		{
			emptyTitle = true;
		}

		// Pass it down to another filter ?
		if ((filteredDoc.getType().length() >= 10) &&
			(filteredDoc.getType().substr(0, 10) == "text/plain"))
		{
			// Do we need to set a default title ?
			if (emptyTitle == true)
			{
				Url urlObj(doc.getLocation());

				// Default to the file name as title
				filteredDoc.setTitle(urlObj.getFile());
#ifdef DEBUG
				cout << "FilterWrapper::filterDocument: set default title " << urlObj.getFile() << endl;
#endif
			}

			// No, it's been reduced to plain text
			filteredDoc.setType(actualType);

			// Take the appropriate action
			docSuccess = action.takeAction(filteredDoc, isNested);
		}
		else
		{
			docSuccess = filterDocument(filteredDoc, actualType, action);
		}

		// Consider indexing anything a success
		if (docSuccess == true)
		{
			finalSuccess = true;
		}

		// Next
		hasDocs = pFilter->has_documents();
	}

	delete pFilter;

#ifdef DEBUG
	cout << "FilterWrapper::filterDocument: done with " << doc.getLocation() << " status " << finalSuccess << endl;
#endif

	return finalSuccess;
}

bool FilterWrapper::unindexNestedDocuments(const string &url)
{
	// Unindex all documents that stem from this file
	return m_pIndex->unindexDocuments(url, IndexInterface::BY_FILE);
}
