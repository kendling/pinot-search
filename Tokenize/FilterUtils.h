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

#ifndef _FILTER_UTILS_H
#define _FILTER_UTILS_H

#include <string>
#include <map>

#include "Document.h"
#include "Filter.h"

/// Utility functions for dealing with Dijon filters.
class FilterUtils
{
	public:
		virtual ~FilterUtils();

		/// Returns a Filter that handles the given MIME type, or one of its parents.
		static Dijon::Filter *getFilter(const std::string &mimeType);

		/// Feeds a document's data to a filter.
		static bool feedFilter(const Document &doc, Dijon::Filter *pFilter);

		/// Populates a document based on metadata extracted by the filter.
		static bool populateDocument(Document &doc, Dijon::Filter *pFilter);

		/// Strips markup from a piece of text.
		static std::string stripMarkup(const std::string &text);

	protected:
		static std::map<std::string, std::string> m_typeAliases;

		FilterUtils();

	private:
		FilterUtils(const FilterUtils &other);
		FilterUtils &operator=(const FilterUtils &other);

};

#endif // _FILTER_UTILS_H
