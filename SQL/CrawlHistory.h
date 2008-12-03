/*
 *  Copyright 2005-2008 Fabrice Colin
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

#ifndef _CRAWL_HISTORY_H
#define _CRAWL_HISTORY_H

#include <time.h>
#include <string>
#include <map>
#include <set>

#include "SQLiteBase.h"

/// Manages crawl history.
class CrawlHistory : public SQLiteBase
{
	public:
		CrawlHistory(const std::string &database);
		virtual ~CrawlHistory();

		typedef enum { UNKNOWN, CRAWLING, CRAWLED, CRAWL_ERROR } CrawlStatus;

		/// Creates the CrawlHistory table in the database.
		static bool create(const std::string &database);

		/// Inserts a source.
		unsigned int insertSource(const std::string &url);

		/// Checks if the source exists.
		bool hasSource(const std::string &url, unsigned int &sourceId);

		/// Returns sources.
		unsigned int getSources(std::map<unsigned int, std::string> &sources);

		/// Deletes a source.
		bool deleteSource(unsigned int sourceId);

		/// Inserts an URL.
		bool insertItem(const std::string &url, CrawlStatus status, unsigned int sourceId,
			time_t date, int errNum = 0);

		/// Checks if an URL is in the history.
		bool hasItem(const std::string &url, CrawlStatus &status, time_t &date);

		/// Updates an URL.
		bool updateItem(const std::string &url, CrawlStatus status, time_t date, int errNum = 0);

		/// Updates URLs.
		bool updateItems(const std::map<std::string, time_t> urls, CrawlStatus status);

		/// Updates the status of items en masse.
		bool updateItemsStatus(CrawlStatus currentStatus, CrawlStatus newStatus,
			unsigned int sourceId, bool allSources = false);

		/// Gets the error number and date for a URL.
		int getErrorDetails(const std::string &url, time_t &date);

		/// Returns items.
		unsigned int getItems(CrawlStatus status, std::set<std::string> &urls);

		/// Returns items that belong to a source.
		unsigned int getSourceItems(unsigned int sourceId, CrawlStatus status,
			std::set<std::string> &urls,
			unsigned int min, unsigned int max,
			time_t minDate = 0);

		/// Returns the number of URLs.
		unsigned int getItemsCount(CrawlStatus status);

		/// Deletes an URL.
		bool deleteItem(const std::string &url);

		/// Deletes all items under a given URL.
		bool deleteItems(const std::string &url);

		/// Deletes URLs belonging to a source.
		bool deleteItems(unsigned int sourceId, CrawlStatus status = UNKNOWN);

		/// Expires items older than the given date.
		bool expireItems(time_t expiryDate);

	protected:
		static std::string statusToText(CrawlStatus status);
		static CrawlStatus textToStatus(const std::string &text);

	private:
		CrawlHistory(const CrawlHistory &other);
		CrawlHistory &operator=(const CrawlHistory &other);

};

#endif // _CRAWL_HISTORY_H
