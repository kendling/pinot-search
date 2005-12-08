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
#include "IndexHistory.h"

IndexHistory::IndexHistory(const string &database) :
	SQLiteBase(database)
{
}

IndexHistory::~IndexHistory()
{
}

/// Creates the necessary tables in the database.
bool IndexHistory::create(const string &database)
{
	bool success = true;

	// The specified path must be a file
	if (SQLiteBase::check(database) == false)
	{
		return false;
	}

	SQLiteBase db(database);

	// Does IndexHistory exist ?
	if (db.executeSimpleStatement("SELECT * FROM IndexHistory LIMIT 1;") == false)
	{
#ifdef DEBUG
		cout << "IndexHistory::create: IndexHistory doesn't exist" << endl;
#endif
		// Create the table
		if (db.executeSimpleStatement("CREATE TABLE IndexHistory \
			(DocId BIGINT UNSIGNED NOT NULL PRIMARY KEY, Title VARCHAR(255), \
			Url VARCHAR(255), Type VARCHAR(255), Language VARCHAR(255), \
			Date TIMESTAMP, Status INT);") == false)
		{
			success = false;
		}
	}

	return success;
}

/// Inserts an item.
bool IndexHistory::insertItem(unsigned int docId, const DocumentInfo &docInfo)
{
	bool success = false;

	// FIXME: make Status configurable
	SQLiteResults *results = executeStatement("INSERT INTO IndexHistory \
		VALUES(%u, '%q', '%q', '%q', '%q', '%d', 1);",
		docId, docInfo.getTitle().c_str(), Url::escapeUrl(docInfo.getLocation()).c_str(),
		docInfo.getType().c_str(), docInfo.getLanguage().c_str(),
		TimeConverter::fromTimestamp(docInfo.getTimestamp()));
	if (results != NULL)
	{
		success = true;
		delete results;
	}

	return success;
}

/// Checks if an URL is in the history; returns the document ID.
unsigned int IndexHistory::hasURL(const string &originalUrl) const
{
	unsigned int docId = 0;

	SQLiteResults *results = executeStatement("SELECT DocId FROM IndexHistory \
		WHERE Url='%q';",
		Url::escapeUrl(originalUrl).c_str());
	if (results != NULL)
	{
		SQLiteRow *row = results->nextRow();
		if (row != NULL)
		{
			docId = (unsigned int)atoi(row->getColumn(0).c_str());

			delete row;
		}

		delete results;
	}

	return docId;
}

/// Updates an item.
bool IndexHistory::updateItem(unsigned int docId, const DocumentInfo &docInfo)
{
	bool success = false;

	// FIXME: make Status configurable
	SQLiteResults *results = executeStatement("UPDATE IndexHistory \
		SET Title='%q', Url='%q', Type='%q', Language='%q', Date='%d' \
		WHERE DocId=%u;",
		docInfo.getTitle().c_str(), Url::escapeUrl(docInfo.getLocation()).c_str(),
		docInfo.getType().c_str(), docInfo.getLanguage().c_str(),
		TimeConverter::fromTimestamp(docInfo.getTimestamp()), docId);
	if (results != NULL)
	{
		success = true;
		delete results;
	}

	return success;
}

/// Lists document IDs; returns the total count.
unsigned int IndexHistory::listItems(set<unsigned int> &items,
	unsigned int maxDocsCount, unsigned int startDoc,
	bool sortByDate) const
{
	string sql;
	unsigned int total = 0;

	sql = "SELECT DocId FROM IndexHistory";
	if (sortByDate == true)
	{
		sql += " ORDER BY Date";
	}
	if (maxDocsCount > 0)
	{
		char docsCountStr[64];

		sql += " LIMIT ";
		if (startDoc > 0)
		{
			snprintf(docsCountStr, 64, "%u", startDoc);
			sql += docsCountStr;
			sql += ",";
		}
		snprintf(docsCountStr, 64, "%u", maxDocsCount);
		sql += docsCountStr;
	}
	sql += ";";
	SQLiteResults *results = executeStatement(sql.c_str());
	if (results != NULL)
	{
		while (results->hasMoreRows() == true)
		{
			SQLiteRow *row = results->nextRow();
			if (row == NULL)
			{
				break;
			}

			unsigned int docId = (unsigned int)atoi(row->getColumn(0).c_str());
#ifdef DEBUG
			cout << "IndexHistory::listItems: item " << docId << endl;
#endif
			items.insert(docId);
			total++;

			delete row;
		}

		delete results;
	}

	return total;
}

/// Gets an item's properties.
bool IndexHistory::getItem(unsigned int docId, DocumentInfo &docInfo) const
{
	bool success = false;

	SQLiteResults *results = executeStatement("SELECT Title, Url, Type, \
		Language, Date FROM IndexHistory WHERE DocId=%u;", docId);
	if (results != NULL)
	{
		SQLiteRow *row =results->nextRow();
		if (row != NULL)
		{
			docInfo.setTitle(row->getColumn(0));
			docInfo.setLocation(Url::unescapeUrl(row->getColumn(1)));
			docInfo.setType(row->getColumn(2));
			docInfo.setLanguage(row->getColumn(3));
			docInfo.setTimestamp(TimeConverter::toTimestamp(atoi(row->getColumn(4).c_str())));
			success = true;

			delete row;
		}

		delete results;
	}

	return success;
}

/// Deletes items.
bool IndexHistory::deleteByURL(const string &originalUrl)
{
	bool success = false;

	SQLiteResults *results = executeStatement("DELETE FROM IndexHistory WHERE Url='%q';",
		Url::escapeUrl(originalUrl).c_str());
	if (results != NULL)
	{
		success = true;
		delete results;
	}

	return success;
}

/// Deletes an item.
bool IndexHistory::deleteItem(unsigned int docId)
{
	bool success = false;

	// Delete from both IndexHistory and DocumentLabels
	SQLiteResults *results = executeStatement("DELETE FROM IndexHistory WHERE DocId=%u;",
		docId);
	if (results != NULL)
	{
		success = true;
		delete results;
	}

	return success;
}
