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

#ifndef _FILTER_WRAPPER_H
#define _FILTER_WRAPPER_H

#include <string>
#include <set>

#include "Document.h"
#include "Visibility.h"
#include "Filter.h"
#include "IndexInterface.h"

/// Takes action on a document that's been reduced to text.
class PINOT_EXPORT ReducedAction
{
	public:
		ReducedAction();
		virtual ~ReducedAction();

		virtual bool takeAction(Document &doc, bool isNested) = 0;

	private:
		ReducedAction(const ReducedAction &other);
		ReducedAction &operator=(const ReducedAction &other);

};

/// A wrapper around Dijon filters.
class PINOT_EXPORT FilterWrapper
{
	public:
		FilterWrapper(IndexInterface *pIndex);
		virtual ~FilterWrapper();

		/// Reduces a document to text.
		bool reduceToText(const Document &doc, ReducedAction &action);

		/// Indexes the given data.
		bool indexDocument(const Document &doc, const std::set<std::string> &labels,
			unsigned int &docId);

		/// Updates the given document.
		bool updateDocument(const Document &doc, unsigned int docId);

		/// Unindexes document(s) at the given location.
		bool unindexDocument(const std::string &location);

	protected:
		IndexInterface *m_pIndex;

		bool filterDocument(const Document &doc, const std::string &originalType,
			ReducedAction &action);

		bool unindexNestedDocuments(const std::string &url);

	private:
		FilterWrapper(const FilterWrapper &other);
		FilterWrapper &operator=(const FilterWrapper &other);

};

#endif // _FILTER_WRAPPER_H
