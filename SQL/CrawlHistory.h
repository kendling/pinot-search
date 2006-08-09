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

#ifndef _CRAWL_HISTORY_H
#define _CRAWL_HISTORY_H

#include <time.h>
#include <string>

#include "SQLiteBase.h"

using namespace std;

class CrawlHistory : public SQLiteBase
{
	public:
		CrawlHistory(const string &database);
		virtual ~CrawlHistory();

		typedef enum { UNKNOWN, CRAWLING, CRAWLED } CrawlStatus;

		/// Creates the CrawlHistory table in the database.
		static bool create(const string &database);

		/// Inserts a source.
		unsigned int insertSource(const string &url);

		/// Checks if the source exists.
		bool hasSource(const string &url, unsigned int &sourceId);

		/// Deletes a source.
		bool deleteSource(unsigned int sourceId);

		/// Inserts an URL.
		bool insertItem(const string &url, CrawlStatus status, unsigned int sourceId, time_t date);

		/// Checks if an URL is in the history.
		bool hasItem(const string &url, CrawlStatus &status, time_t &date) const;

		/// Updates an URL.
		bool updateItem(const string &url, CrawlStatus status, time_t date);

		/// Returns the number of URLs.
		unsigned int getItemsCount(void) const;

		/// Checks if an URL is in the history.
		bool hasItem(const string &url) const;

		/// Deletes an URL.
		bool deleteItem(const string &url);

		/// Deletes URLs belonging to a source.
		bool deleteItems(unsigned int sourceId);

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
