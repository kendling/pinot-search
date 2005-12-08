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
#include "TimeConverter.h"
#include "QueryHistory.h"

QueryHistory::QueryHistory(const string &database) :
	SQLiteBase(database)
{
}

QueryHistory::~QueryHistory()
{
}

/// Creates the QueryHistory table in the database.
bool QueryHistory::create(const string &database)
{
	bool success = true;

	// The specified path must be a file
	if (SQLiteBase::check(database) == false)
	{
		return false;
	}

	SQLiteBase db(database);

	// Does QueryHistory exist ?
	if (db.executeSimpleStatement("SELECT * FROM QueryHistory LIMIT 1;") == false)
	{
#ifdef DEBUG
		cout << "QueryHistory::create: QueryHistory doesn't exist" << endl;
#endif
		// Create the table
		if (db.executeSimpleStatement("CREATE TABLE QueryHistory (QueryName VARCHAR(255), \
			EngineName VARCHAR(255), HostName VARCHAR(255), Url VARCHAR(255), Title VARCHAR(255), \
			Extract VARCHAR(255), Language VARCHAR(255), Score FLOAT, PrevScore FLOAT, Date INTEGER, \
			PRIMARY KEY(QueryName, EngineName, Url));") == false)
		{
			success = false;
		}
	}

	return success;
}

/// Inserts an URL.
bool QueryHistory::insertItem(const string &queryName, const string &engineName, const string &url,
	const string &title, const string &extract, const string &language, float score)
{
	Url urlObj(url);
	string hostName = urlObj.getHost();
	string escapedUrl = Url::escapeUrl(url);
	bool success = false;

#ifdef DEBUG
	cout << "QueryHistory::insertItem: called" << endl;
#endif
	SQLiteResults *results = executeStatement("INSERT INTO QueryHistory \
		VALUES('%q', '%q', '%q', '%q', '%q', '%q', '%q', '%f', '0.0', '%d');",
		queryName.c_str(), engineName.c_str(), hostName.c_str(),
		escapedUrl.c_str(), title.c_str(), extract.c_str(), language.c_str(),
		score, time(NULL));
	if (results != NULL)
	{
		success = true;
		delete results;
	}

	return success;
}

/// Checks if an URL is in the history; returns its current score or 0 if not found.
float QueryHistory::hasItem(const string &queryName, const string &engineName, const string &url,
	float &previousScore) const
{
	float score = 0;

	SQLiteResults *results = executeStatement("SELECT Score, PrevScore FROM QueryHistory \
		WHERE QueryName='%q' AND EngineName='%q' AND Url='%q';",
		queryName.c_str(), engineName.c_str(), Url::escapeUrl(url).c_str());
	if (results != NULL)
	{
		SQLiteRow *row = results->nextRow();
		if (row != NULL)
		{
			score = (float)atof(row->getColumn(0).c_str());
			previousScore = (float)atof(row->getColumn(1).c_str());

			delete row;
		}

		delete results;
	}

	return score;
}

/// Updates an URL's details.
bool QueryHistory::updateItem(const string &queryName, const string &engineName, const string &url,
	const string &title, const string &extract, const string &language, float score)
{
	bool success = false;

#ifdef DEBUG
	cout << "QueryHistory::updateItem: called on " << url << endl;
#endif
	SQLiteResults *results = executeStatement("UPDATE QueryHistory SET PrevScore=Score, \
		Score=%f, Date='%d', Title='%q', Extract='%q', Language='%q' \
		WHERE QueryName='%q' AND EngineName='%q' AND Url='%q';",
		score, time(NULL), title.c_str(), extract.c_str(), language.c_str(),
		queryName.c_str(), engineName.c_str(), Url::escapeUrl(url).c_str());
	if (results != NULL)
	{
		success = true;
		delete results;
	}

	return success;
}

/// Gets the first max items for the given query, engine pair.
bool QueryHistory::getItems(const string &queryName, const string &engineName,
	unsigned int max, vector<Result> &resultsList) const
{
	bool success = false;

	SQLiteResults *results = executeStatement("SELECT Url, Title, Extract, Language, Score \
		FROM QueryHistory WHERE QueryName='%q' AND EngineName='%q' ORDER BY Score DESC \
		LIMIT %u;", queryName.c_str(), engineName.c_str(), max);
	if (results != NULL)
	{
		while (results->hasMoreRows() == true)
		{
			SQLiteRow *row = results->nextRow();
			if (row == NULL)
			{
				break;
			}

			Result result(Url::unescapeUrl(row->getColumn(0)).c_str(),
				row->getColumn(1),
				row->getColumn(2),
				row->getColumn(3),
				(float)atof(row->getColumn(4).c_str()));
			resultsList.push_back(result);
			success = true;

			delete row;
		}

		delete results;
	}

	return success;
}

/// Gets an item's extract.
string QueryHistory::getItemExtract(const string &queryName, const string &engineName, const string &url) const
{
	string extract;

	SQLiteResults *results = executeStatement("SELECT Extract FROM QueryHistory \
		WHERE QueryName='%q' AND EngineName='%q' AND Url='%q';",
		queryName.c_str(), engineName.c_str(), Url::escapeUrl(url).c_str());
	if (results != NULL)
	{
		SQLiteRow *row = results->nextRow();
		if (row != NULL)
		{
			extract = row->getColumn(0);

			delete row;
		}

		delete results;
	}

	return extract;
}

/// Gets a query's last run time.
string QueryHistory::getLastRun(const string &queryName, const string &engineName) const
{
	SQLiteResults *results = NULL;
	string lastRun;

	if (queryName.empty() == true)
	{
		return lastRun;
	}

	if (engineName.empty() == true)
	{
		results = executeStatement("SELECT MAX(Date) FROM QueryHistory \
			WHERE QueryName='%q';", queryName.c_str());
	}
	else
	{
		results = executeStatement("SELECT MAX(Date) FROM QueryHistory \
			WHERE QueryName='%q' AND EngineName='%q';",
			queryName.c_str(), engineName.c_str());
	}

	if (results != NULL)
	{
		SQLiteRow *row = results->nextRow();
		if (row != NULL)
		{
			int latestDate = atoi(row->getColumn(0).c_str());
			if (latestDate > 0)
			{
				lastRun = TimeConverter::toTimestamp((time_t)latestDate);
			}

			delete row;
		}

		delete results;
	}

	return lastRun;
}

/// Deletes items.
bool QueryHistory::deleteItems(const string &name, bool isQueryName)
{
	SQLiteResults *results = NULL;

	if (isQueryName == true)
	{
		results = executeStatement("DELETE FROM QueryHistory \
			WHERE QueryName='%q';", name.c_str());
	}
	else
	{
		results = executeStatement("DELETE FROM QueryHistory \
			WHERE EngineName='%q';", name.c_str());
	}

	if (results != NULL)
	{
		delete results;

		return true;
	}

	return false;
}
