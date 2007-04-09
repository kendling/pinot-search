/*
 *  Copyright 2005,2006 Fabrice Colin
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

#ifndef _QUERY_HISTORY_H
#define _QUERY_HISTORY_H

#include <string>
#include <vector>

#include "DocumentInfo.h"
#include "SQLiteBase.h"

using namespace std;

/// Manages query history.
class QueryHistory : public SQLiteBase
{
	public:
		QueryHistory(const string &database);
		virtual ~QueryHistory();

		/// Creates the QueryHistory table in the database.
		static bool create(const string &database);

		/// Inserts an URL.
		bool insertItem(const string &queryName, const string &engineName, const string &url,
			const string &title, const string &extract, const string &charset, float score);

		/**
		  * Checks if an URL is in the query's history.
		  * If it is, it returns the current and previous scores; returns 0 if not found.
		  */
		float hasItem(const string &queryName, const string &engineName, const string &url,
			float &previousScore) const;

		/// Updates an URL's details.
		bool updateItem(const string &queryName, const string &engineName, const string &url,
			const string &title, const string &extract, const string &charset, float score);

		/// Gets the first max items for the given query, engine pair.
		bool getItems(const string &queryName, const string &engineName,
			unsigned int max, vector<DocumentInfo> &resultsList) const;

		/// Gets an item's extract.
		string getItemExtract(const string &queryName, const string &engineName,
			const string &url, string &charset) const;

		/// Gets a query's last run time.
		string getLastRun(const string &queryName, const string &engineName = "") const;

		/// Deletes items.
		bool deleteItems(const string &name, bool isQueryName);

		/// Expires items older than the given date.
		bool expireItems(time_t expiryDate);

	private:
		QueryHistory(const QueryHistory &other);
		QueryHistory &operator=(const QueryHistory &other);

};

#endif // _QUERY_HISTORY_H
