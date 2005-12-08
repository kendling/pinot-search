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
#include "ActionHistory.h"

ActionHistory::ActionHistory(const string &database) :
	SQLiteBase(database)
{
}

ActionHistory::~ActionHistory()
{
}

/// Creates the ActionHistory table in the database.
bool ActionHistory::create(const string &database)
{
	bool success = true;

	// The specified path must be a file
	if (SQLiteBase::check(database) == false)
	{
		return false;
	}

	SQLiteBase db(database);

	// Does ActionHistory exist ?
	if (db.executeSimpleStatement("SELECT * FROM ActionHistory LIMIT 1;") == false)
	{
#ifdef DEBUG
		cout << "ActionHistory::create: ActionHistory doesn't exist" << endl;
#endif
		// Create the table
		if (db.executeSimpleStatement("CREATE TABLE ActionHistory (Type UNSIGNED INT NOT NULL, Date TIMESTAMP, Option TEXT, PRIMARY KEY(Type, Option));") == false)
		{
			success = false;
		}
	}

	return success;
}

/// Inserts an item.
bool ActionHistory::insertItem(ActionType type, const string &option)
{
	string date = TimeConverter::toTimestamp(time(NULL));
	bool success = false;

	SQLiteResults *results = executeStatement("INSERT INTO ActionHistory \
		VALUES(%u, '%q', '%q');", (unsigned int)type, date.c_str(), option.c_str());
	if (results != NULL)
	{
		success = true;
		delete results;
	}

	return success;
}

/// Gets and deletes the oldest item.
bool ActionHistory::deleteOldestItem(ActionType &type, string &option)
{
	string date;
	bool success = false;

	if (getOldestItem(type, date, option) == false)
	{
		return false;
	}

	// Delete from ActionHistory
	SQLiteResults *results = executeStatement("DELETE FROM ActionHistory \
		WHERE Type=%u AND Option='%q';", (unsigned int)type, option.c_str());
	if (results != NULL)
	{
		success = true;
		delete results;
	}

	return success;
}

bool ActionHistory::getOldestItem(ActionType &type, string &date, string &option) const
{
	bool success = false;

	SQLiteResults *results = executeStatement("SELECT Type, Date, Option \
		FROM ActionHistory ORDER BY %s DESC LIMIT 1", "Date");
	if (results != NULL)
	{
		SQLiteRow *row = results->nextRow();
		if (row != NULL)
		{
			type = (ActionType)atoi(row->getColumn(0).c_str());
			date = row->getColumn(1);
			option = row->getColumn(2);
			success = true;

			delete row;
		}

		delete results;
	}

	return success;
}
