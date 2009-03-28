/*
 *  Copyright 2007-2009 Fabrice Colin
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
#include "FilterWrapper.h"

using std::cout;
using std::endl;
using std::string;
using std::set;
using namespace Dijon;

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

bool FilterWrapper::indexDocument(const Document &doc, const set<string> &labels, unsigned int &docId)
{
	string originalType(doc.getType());

	if (m_pIndex == NULL)
	{
		return false;
	}

	unindexNestedDocuments(doc.getLocation());

	IndexAction action(m_pIndex, labels, docId, false);

	bool filteredDoc = FilterUtils::filterDocument(doc, originalType, action);
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

	return FilterUtils::filterDocument(doc, originalType, action);
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

bool FilterWrapper::unindexNestedDocuments(const string &url)
{
	// Unindex all documents that stem from this file
	return m_pIndex->unindexDocuments(url, IndexInterface::BY_FILE);
}
