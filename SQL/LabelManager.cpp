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
#include "LabelManager.h"

LabelManager::LabelManager(const string &database) :
	SQLiteBase(database)
{
}

LabelManager::~LabelManager()
{
}

unsigned int LabelManager::getLabelId(const string &labelName) const
{
	unsigned int labelId = 0;

	// Get the label ID
	SQLiteResults *results = executeStatement("SELECT LabelId 	FROM IndexLabels WHERE Name='%q';",
		labelName.c_str());
	if (results != NULL)
	{
		SQLiteRow *row =results->nextRow();
		if (row != NULL)
		{
			labelId = (unsigned int)atoi(row->getColumn(0).c_str());

			delete row;
		}

		delete results;
	}

	return labelId;
}

unsigned int LabelManager::getNewLabelId(void) const
{
	unsigned int labelId = 1;

	// Get the label ID
	SQLiteResults *results = executeStatement("SELECT ROWID FROM IndexLabels \
		ORDER BY ROWID DESC LIMIT 1;");
	if (results != NULL)
	{
		SQLiteRow *row =results->nextRow();
		if (row != NULL)
		{
			labelId = (unsigned int)atoi(row->getColumn(0).c_str());
			++labelId;

			delete row;
		}

		delete results;
	}

	return labelId;
}

/// Creates the necessary tables in the database.
bool LabelManager::create(const string &database)
{
	bool success = true;

	// The specified path must be a file
	if (SQLiteBase::check(database) == false)
	{
		return false;
	}

	SQLiteBase db(database);

	// Does IndexLabels exist ?
	if (db.executeSimpleStatement("SELECT * FROM IndexLabels LIMIT 1;") == false)
	{
#ifdef DEBUG
		cout << "LabelManager::create: IndexLabels doesn't exist" << endl;
#endif
		// Create the table
		if (db.executeSimpleStatement("CREATE TABLE IndexLabels \
			(LabelId BIGINT UNSIGNED NOT NULL PRIMARY KEY, Name VARCHAR(255));") == false)
		{
			success = false;
		}
	}
	// Does DocumentLabels exist ?
	if (db.executeSimpleStatement("SELECT * FROM DocumentLabels LIMIT 1;") == false)
	{
#ifdef DEBUG
		cout << "LabelManager::create: DocumentLabels doesn't exist" << endl;
#endif
		// Create the table
		if (db.executeSimpleStatement("CREATE TABLE DocumentLabels \
			(LabelId BIGINT UNSIGNED NOT NULL, DocId BIGINT UNSIGNED NOT NULL, \
			Source VARCHAR(255), PRIMARY KEY(LabelId, DocId, Source));") == false)
		{
			success = false;
		}
	}

	return success;
}

/// Gets a list of items with the given label.
bool LabelManager::getDocumentsWithLabel(const string &labelName, const string &sourceName,
	set<unsigned int> &items) const
{
	bool success = false;

	unsigned int labelId = getLabelId(labelName);
	if (labelId == 0)
	{
		// Label was not found
		return false;
	}

	SQLiteResults *results = executeStatement("SELECT DocId FROM DocumentLabels \
		WHERE LabelId=%u AND Source='%q';",
		labelId, sourceName.c_str());
	if (results != NULL)
	{
		while (results->hasMoreRows() == true)
		{
			SQLiteRow *row =results->nextRow();
			if (row == NULL)
			{
				break;
			}

			unsigned int docId = (unsigned int)atoi(row->getColumn(0).c_str());
#ifdef DEBUG
			cout << "LabelManager::getDocumentsWithLabel: item " << docId << endl;
#endif
			items.insert(docId);
			success = true;

			delete row;
		}

		delete results;
	}

	return success;
}

/// Checks if a document has a label.
bool LabelManager::hasLabel(unsigned int docId, const string &sourceName,
	const string &labelName) const
{
	bool success = false;

	unsigned int labelId = getLabelId(labelName);
	if (labelId == 0)
	{
		// Label was not found
		return false;
	}

	SQLiteResults *results = executeStatement("SELECT DocId FROM DocumentLabels \
		WHERE LabelId=%u AND DocId=%u AND Source='%q';",
		labelId, docId, sourceName.c_str());
	if (results != NULL)
	{
		SQLiteRow *row =results->nextRow();
		if (row != NULL)
		{
			// Yes, this document has the given label
			success = true;

			delete row;
		}

		delete results;
	}

	return success;
}

/// Sets a document's labels.
bool LabelManager::setLabels(unsigned int docId, const string &sourceName,
	const set<string> &labels)
{
	bool success = false;

	// First off, delete all labels for this document
	SQLiteResults *results = executeStatement("DELETE FROM DocumentLabels \
		WHERE DocId=%u AND Source='%q';",
		docId, sourceName.c_str());
	if (results == NULL)
	{
		return false;
	}
	delete results;

	for (set<string>::const_iterator iter = labels.begin(); iter != labels.end(); ++iter)
	{
		string labelName = (*iter);

#ifdef DEBUG
		cout << "LabelManager::setLabels: label " << labelName << endl;
#endif
		// Does this label exist ?
		unsigned int labelId = getLabelId(labelName);
		if (labelId == 0)
		{
			// No, it doesn't : create it then
			labelId = getNewLabelId();
			results = executeStatement("INSERT INTO IndexLabels VALUES(%u, '%q');",
				labelId, labelName.c_str());
			if (results == NULL)
			{
#ifdef DEBUG
				cout << "LabelManager::setLabels: couldn't create label " << labelName << endl;
#endif
				continue;
			}
			delete results;
		}

		// Insert this label
		SQLiteResults *results = executeStatement("INSERT INTO DocumentLabels VALUES(%u, %u, '%q');",
			labelId, docId, sourceName.c_str());
		if (results != NULL)
		{
			delete results;
		}
	}

	return true;
}

/// Gets the labels for the given document.
bool LabelManager::getLabels(unsigned int docId, const string &sourceName,
	set<string> &labels) const
{
	bool success = false;

	SQLiteResults *results = executeStatement("SELECT i.Name FROM IndexLabels i, \
		DocumentLabels d WHERE d.LabelId=i.LabelId AND d.DocId=%u AND d.Source='%q';",
		docId, sourceName.c_str());
	if (results != NULL)
	{
		while (results->hasMoreRows() == true)
		{
			SQLiteRow *row = results->nextRow();
			if (row == NULL)
			{
				break;
			}

			string labelName = row->getColumn(0);
#ifdef DEBUG
			cout << "LabelManager::getLabels: label " << labelName << endl;
#endif
			labels.insert(labelName);
			success = true;

			delete row;
		}

		delete results;
	}

	return success;
}

/// Renames a label.
bool LabelManager::renameLabel(const string &name, const string &newName)
{
	bool success = false;

	SQLiteResults *results = executeStatement("UPDATE IndexLabels SET Name='%q' \
		WHERE Name='%q';",
		newName.c_str(), name.c_str());
	if (results != NULL)
	{
		success = true;
		delete results;
	}
#ifdef DEBUG
	cout << "LabelManager::renameLabel: renamed " << name << " to " << newName << endl;
#endif

	return success;
}

/// Deletes all references to a label.
bool LabelManager::deleteLabel(const string &name)
{
	bool success = false;

	unsigned int labelId = getLabelId(name);
	if (labelId == 0)
	{
		// Nothing to delete
		return true;
	}

	// Delete from both IndexLabels and DocumentLabels
	SQLiteResults *results = executeStatement("DELETE FROM DocumentLabels \
		WHERE LabelId=%u; DELETE FROM IndexLabels WHERE LabelId=%u;",
		labelId, labelId);
	if (results != NULL)
	{
		success = true;
		delete results;
	}

	return success;
}

/// Deletes an item.
bool LabelManager::deleteItem(unsigned int docId, const string &sourceName)
{
	bool success = false;

	// Delete from both IndexHistory and DocumentLabels
	SQLiteResults *results = executeStatement("DELETE FROM DocumentLabels WHERE DocId=%u \
		AND Source='%q';",
		docId, sourceName.c_str());
	if (results != NULL)
	{
		success = true;
		delete results;
	}

	return success;
}
