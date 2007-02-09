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

#ifndef _FILTER_WRAPPER_H
#define _FILTER_WRAPPER_H

#include <string>
#include <set>

#include "Document.h"
#include "Filter.h"
#include "IndexInterface.h"

/// A wrapper around Dijon filters.
class FilterWrapper
{
	public:
		virtual ~FilterWrapper();

		/// Feeds a document's data to a filter.
		static bool feedFilter(const Document &doc, Dijon::Filter *pFilter);

		/// Strips markup from a piece of text.
		static std::string stripMarkup(const std::string &text);

		/// Indexes the given data.
		static bool indexDocument(IndexInterface &index, const Document &doc,
			const std::set<std::string> &labels, unsigned int &docId);

		/// Updates the given document.
		static bool updateDocument(unsigned int docId, IndexInterface &index,
			const Document &doc);

	protected:
		FilterWrapper();

		static bool filterDocument(IndexInterface &index, Document &doc,
			unsigned int count, const std::set<std::string> &labels, unsigned int &docId,
			bool doUpdate);

	private:
		FilterWrapper(const FilterWrapper &other);
		FilterWrapper &operator=(const FilterWrapper &other);

};

#endif // _FILTER_WRAPPER_H
