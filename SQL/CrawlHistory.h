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

#ifndef _CRAWL_HISTORY_H
#define _CRAWL_HISTORY_H

#include <time.h>
#include <string>
#include <map>
#include <set>

#include "SQLiteBase.h"

using namespace std;

/// Manages crawl history.
class CrawlHistory : public SQLiteBase
{
	public:
		CrawlHistory(const string &database);
		virtual ~CrawlHistory();

		typedef enum { UNKNOWN, CRAWLING, CRAWLED, ERROR } CrawlStatus;

		/// Creates the CrawlHistory table in the database.
		static bool create(const string &database);

		/// Inserts a source.
		unsigned int insertSource(const string &url);

		/// Checks if the source exists.
		bool hasSource(const string &url, unsigned int &sourceId);

		/// Returns sources.
		unsigned int getSources(map<unsigned int, string> &sources);

		/// Deletes a source.
		bool deleteSource(unsigned int sourceId);

		/// Inserts an URL.
		bool insertItem(const string &url, CrawlStatus status, unsigned int sourceId,
			time_t date, int errNum = 0);

		/// Checks if an URL is in the history.
		bool hasItem(const string &url, CrawlStatus &status, time_t &date);

		/// Updates an URL.
		bool updateItem(const string &url, CrawlStatus status, time_t date, int errNum = 0);

		/// Updates URLs.
		bool updateItems(const map<string, time_t> urls, CrawlStatus status);

		/// Updates the status of items en masse.
		bool updateItemsStatus(unsigned int sourceId, CrawlStatus currentStatus, CrawlStatus newStatus);

		/// Gets the error number and date for a URL.
		int getErrorDetails(const string &url, time_t &date);

		/// Returns items that belong to a source.
		unsigned int getSourceItems(unsigned int sourceId, CrawlStatus status,
			set<string> &urls, time_t minDate = 0);

		/// Returns the number of URLs.
		unsigned int getItemsCount(CrawlStatus status);

		/// Deletes an URL.
		bool deleteItem(const string &url);

		/// Deletes all items under a given URL.
		bool deleteItems(const string &url);

		/// Deletes URLs belonging to a source.
		bool deleteItems(unsigned int sourceId, CrawlStatus status = UNKNOWN);

		/// Expires items older than the given date.
		bool expireItems(time_t expiryDate);

	protected:
		static string statusToText(CrawlStatus status);
		static CrawlStatus textToStatus(const string &text);

	private:
		CrawlHistory(const CrawlHistory &other);
		CrawlHistory &operator=(const CrawlHistory &other);

};

#endif // _CRAWL_HISTORY_H
