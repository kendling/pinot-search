/*
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

#ifndef _INDEX_HISTORY_H
#define _INDEX_HISTORY_H

#include <string>
#include <map>
#include <set>

#include "DocumentInfo.h"
#include "SQLiteBase.h"

using namespace std;

class IndexHistory : public SQLiteBase
{
	public:
		IndexHistory(const string &database);
		virtual ~IndexHistory();

		/// Creates the necessary tables in the database.
		static bool create(const string &database);

		/// Inserts an item.
		bool insertItem(unsigned int docId, const DocumentInfo &docInfo);

		/// Checks if an URL is in the history; returns the document ID.
		unsigned int hasURL(const string &originalUrl) const;

		/// Updates an item.
		bool updateItem(unsigned int docId, const DocumentInfo &docInfo);

		/// Lists document IDs; returns the total count.
		unsigned int listItems(set<unsigned int> &items,
			unsigned int maxDocsCount = 0, unsigned int startDoc = 0,
			bool sortByDate = false) const;

		/// Gets an item's properties.
		bool getItem(unsigned int docId, DocumentInfo &docInfo) const;

		/// Deletes items.
		bool deleteByURL(const string &originalUrl);

		/// Deletes an item.
		bool deleteItem(unsigned int docId);

	private:
		IndexHistory(const IndexHistory &other);
		IndexHistory &operator=(const IndexHistory &other);

};

#endif // _INDEX_HISTORY_H
