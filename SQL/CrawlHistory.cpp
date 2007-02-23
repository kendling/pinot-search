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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>

#include "Url.h"
#include "CrawlHistory.h"

CrawlHistory::CrawlHistory(const string &database) :
	SQLiteBase(database)
{
}

CrawlHistory::~CrawlHistory()
{
}

string CrawlHistory::statusToText(CrawlStatus status)
{
	string text;

	switch (status)
	{
		case UNKNOWN:
			text = "UNKNOWN";
			break;
		case CRAWLING:
			text = "CRAWLING";
			break;
		case CRAWLED:
			text = "CRAWLED";
			break;
		case ERROR:
			text = "ERROR";
			break;
		default:
			break;
	}

	return text;
}

CrawlHistory::CrawlStatus CrawlHistory::textToStatus(const string &text)
{
	CrawlStatus status = UNKNOWN;

	if (text == "CRAWLING")
	{
		status = CRAWLING;
	}
	else if (text == "CRAWLED")
	{
		status = CRAWLED;
	}
	else if (text == "ERROR")
	{
		status = ERROR;
	}

	return status;
}

/// Creates the CrawlHistory table in the database.
bool CrawlHistory::create(const string &database)
{
	// The specified path must be a file
	if (SQLiteBase::check(database) == false)
	{
		return false;
	}

	SQLiteBase db(database);

	// Does CrawlSources exist ?
	if (db.executeSimpleStatement("SELECT * FROM CrawlSources LIMIT 1;") == false)
	{
		if (db.executeSimpleStatement("CREATE TABLE CrawlSources (SourceID INTEGER \
			PRIMARY KEY, Url VARCHAR(255));") == false)
		{
			return false;
		}
	}

	// Does CrawlHistory exist ?
	if (db.executeSimpleStatement("SELECT * FROM CrawlHistory LIMIT 1;") == false)
	{
		if (db.executeSimpleStatement("CREATE TABLE CrawlHistory (Url VARCHAR(255) \
			PRIMARY KEY, Status VARCHAR(255), SourceID INTEGER, DATE INTEGER);") == false)
		{
			return false;
		}
	}

	return true;
}

/// Inserts a source.
unsigned int CrawlHistory::insertSource(const string &url)
{
	unsigned int sourceId = 0;

	SQLiteResults *results = executeStatement("SELECT MAX(SourceID) FROM CrawlSources;");
	if (results != NULL)
	{
		SQLiteRow *row = results->nextRow();
		if (row != NULL)
		{
			sourceId = atoi(row->getColumn(0).c_str());
			++sourceId;

			delete row;
		}

		delete results;
	}

	results = executeStatement("INSERT INTO CrawlSources \
		VALUES('%u', '%q');",
		sourceId, Url::escapeUrl(url).c_str());
	if (results != NULL)
	{
		delete results;
	}

	return sourceId;
}

/// Checks if the source exists.
bool CrawlHistory::hasSource(const string &url, unsigned int &sourceId)
{
	bool success = false;

	SQLiteResults *results = executeStatement("SELECT SourceID FROM CrawlSources \
		WHERE Url='%q';", Url::escapeUrl(url).c_str());
	if (results != NULL)
	{
		SQLiteRow *row = results->nextRow();
		if (row != NULL)
		{
			sourceId = atoi(row->getColumn(0).c_str());
			success = true;

			delete row;
		}

		delete results;
	}

	return success;
}

/// Returns sources.
unsigned int CrawlHistory::getSources(map<unsigned int, string> &sources) const
{
	unsigned int count = 0;

	SQLiteResults *results = executeStatement("SELECT SourceID, Url FROM CrawlSources;");
	if (results != NULL)
	{
		while (results->hasMoreRows() == true)
		{
			SQLiteRow *row = results->nextRow();
			if (row == NULL)
			{
				break;
			}

			sources[(unsigned int)atoi(row->getColumn(0).c_str())] = Url::unescapeUrl(row->getColumn(1));
			++count;

			delete row;
		}

		delete results;
	}

	return count;
}

/// Deletes a source.
bool CrawlHistory::deleteSource(unsigned int sourceId)
{
	bool success = false;

	SQLiteResults *results = executeStatement("DELETE FROM CrawlSources \
		WHERE SourceID='%u';", sourceId);
	if (results != NULL)
	{
		success = true;
		delete results;
	}

	return success;
}

/// Inserts an URL.
bool CrawlHistory::insertItem(const string &url, CrawlStatus status, unsigned int sourceId, time_t date)
{
	bool success = false;

	SQLiteResults *results = executeStatement("INSERT INTO CrawlHistory \
		VALUES('%q', '%q', '%u', '%d');",
		Url::escapeUrl(url).c_str(), statusToText(status).c_str(), sourceId,
		(date == 0 ? time(NULL) : date));
	if (results != NULL)
	{
		success = true;
		delete results;
	}

	return success;
}

/// Checks if an URL is in the history.
bool CrawlHistory::hasItem(const string &url, CrawlStatus &status, time_t &date) const
{
	bool success = false;

	SQLiteResults *results = executeStatement("SELECT Status, Date FROM CrawlHistory \
		WHERE Url='%q';", Url::escapeUrl(url).c_str());
	if (results != NULL)
	{
		SQLiteRow *row = results->nextRow();
		if (row != NULL)
		{
			status = textToStatus(row->getColumn(0));
			date = (time_t)atoi(row->getColumn(1).c_str());
			success = true;

			delete row;
		}

		delete results;
	}

	return success;
}

/// Updates an URL.
bool CrawlHistory::updateItem(const string &url, CrawlStatus status, time_t date)
{
	bool success = false;

	SQLiteResults *results = executeStatement("UPDATE CrawlHistory \
		SET Status='%q', Date='%d' WHERE Url='%q';",
		statusToText(status).c_str(), (date == 0 ? time(NULL) : date),
		Url::escapeUrl(url).c_str());
	if (results != NULL)
	{
		success = true;
		delete results;
	}

	return success;
}

/// Updates the status of items en masse.
bool CrawlHistory::updateItemsStatus(unsigned int sourceId, CrawlStatus currentStatus, CrawlStatus newStatus)
{
	bool success = false;

	SQLiteResults *results = executeStatement("UPDATE CrawlHistory \
		SET Status='%q' WHERE SourceId='%u' AND Status='%q';",
		statusToText(newStatus).c_str(), sourceId,
		statusToText(currentStatus).c_str());
	if (results != NULL)
	{
		success = true;
		delete results;
	}

	return success;
}

/// Returns items that belong to a source.
unsigned int CrawlHistory::getSourceItems(unsigned int sourceId, CrawlStatus status,
	set<string> &urls, time_t maxDate) const
{
	SQLiteResults *results = NULL;
	unsigned int count = 0;

	if (maxDate > 0)
	{
		results = executeStatement("SELECT Url FROM CrawlHistory \
			WHERE SourceId='%u' AND Status='%q' AND Date<'%d';",
			sourceId, statusToText(status).c_str(), maxDate);
	}
	else
	{
		// Ignore the date
		results = executeStatement("SELECT Url FROM CrawlHistory \
			WHERE SourceId='%u' AND Status='%q';",
			sourceId, statusToText(status).c_str());
	}

	if (results != NULL)
	{
		while (results->hasMoreRows() == true)
		{
			SQLiteRow *row = results->nextRow();
			if (row == NULL)
			{
				break;
			}

			urls.insert(Url::unescapeUrl(row->getColumn(0)));
			++count;

			delete row;
		}

		delete results;
	}

	return count;
}

/// Returns the number of URLs.
unsigned int CrawlHistory::getItemsCount(CrawlStatus status) const
{
	unsigned int count = 0;

	SQLiteResults *results = executeStatement("SELECT COUNT(*) FROM CrawlHistory WHERE Status='%q';",
		statusToText(status).c_str());
	if (results != NULL)
	{
		SQLiteRow *row = results->nextRow();
		if (row != NULL)
		{
			count = atoi(row->getColumn(0).c_str());

			delete row;
		}

		delete results;
	}

	return count;
}

/// Deletes an URL.
bool CrawlHistory::deleteItem(const string &url)
{
	bool success = false;

	SQLiteResults *results = executeStatement("DELETE FROM CrawlHistory \
		WHERE Url='%q';", Url::escapeUrl(url).c_str());
	if (results != NULL)
	{
		success = true;
		delete results;
	}

	return success;
}

/// Deletes all items under a given URL.
bool CrawlHistory::deleteItems(const string &url)
{
	bool success = false;

	SQLiteResults *results = executeStatement("DELETE FROM CrawlHistory \
		WHERE Url LIKE '%q%%';", Url::escapeUrl(url).c_str());
	if (results != NULL)
	{
		success = true;
		delete results;
	}

	return success;
}

/// Deletes URLs belonging to a source.
bool CrawlHistory::deleteItems(unsigned int sourceId, CrawlStatus status)
{
	SQLiteResults *results = NULL;
	bool success = false;

	if (status == UNKNOWN)
	{
		results = executeStatement("DELETE FROM CrawlHistory \
			WHERE SourceID='%u';", sourceId);
	}
	else
	{
		results = executeStatement("DELETE FROM CrawlHistory \
			WHERE SourceID='%u' AND Status='%q';",
			sourceId, statusToText(status).c_str());
	}

	if (results != NULL)
	{
		success = true;
		delete results;
	}

	return success;
}

/// Expires items older than the given date.
bool CrawlHistory::expireItems(time_t expiryDate)
{
	bool success = false;

	SQLiteResults *results = executeStatement("DELETE FROM CrawlHistory \
		WHERE Date<'%d';", expiryDate);
	if (results != NULL)
	{
		success = true;
		delete results;
	}

	return success;
}
