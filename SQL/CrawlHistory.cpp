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
bool CrawlHistory::insertItem(const string &url, CrawlStatus status, unsigned int sourceId)
{
	bool success = false;

	SQLiteResults *results = executeStatement("INSERT INTO CrawlHistory \
		VALUES('%q', '%q', '%u', '%d');",
		Url::escapeUrl(url).c_str(), statusToText(status).c_str(), sourceId, time(NULL));
	if (results != NULL)
	{
		success = true;
		delete results;
	}

	return success;
}

/// Checks if an URL is in the history.
bool CrawlHistory::hasItem(const string &url, CrawlStatus &status) const
{
	bool success = false;

	SQLiteResults *results = executeStatement("SELECT Status FROM CrawlHistory \
		WHERE Url='%q';", Url::escapeUrl(url).c_str());
	if (results != NULL)
	{
		SQLiteRow *row = results->nextRow();
		if (row != NULL)
		{
			status = textToStatus(row->getColumn(0));
			success = true;

			delete row;
		}

		delete results;
	}

	return success;
}

/// Updates an URL.
bool CrawlHistory::updateItem(const string &url, CrawlStatus status)
{
	bool success = false;

	SQLiteResults *results = executeStatement("UPDATE CrawlHistory \
		SET Status='%q', Date='%d' WHERE Url='%q';",
		statusToText(status).c_str(), Url::escapeUrl(url).c_str(), time(NULL));
	if (results != NULL)
	{
		success = true;
		delete results;
	}

	return success;
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

/// Deletes URLs belonging to a source.
bool CrawlHistory::deleteItems(unsigned int sourceId)
{
	bool success = false;

	SQLiteResults *results = executeStatement("DELETE FROM CrawlHistory \
		WHERE SourceID='%u';", sourceId);
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
