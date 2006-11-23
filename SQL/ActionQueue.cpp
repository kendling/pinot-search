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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>

#include "Url.h"
#include "StringManip.h"
#include "TimeConverter.h"
#include "ActionQueue.h"

using std::string;
using std::cout;
using std::endl;

ActionQueue::ActionQueue(const string &database, const string queueId) :
	SQLiteBase(database),
	m_queueId(queueId)
{
}

ActionQueue::~ActionQueue()
{
}

string ActionQueue::typeToText(ActionType type)
{
	string text;

	switch (type)
	{
		case INDEX:
			text = "INDEX";
			break;
		case UNINDEX:
			text = "UNINDEX";
			break;
		default:
			break;
	}

	return text;
}

ActionQueue::ActionType ActionQueue::textToType(const string &text)
{
	ActionType type = INDEX;

	if (text == "UNINDEX")
	{
		type = UNINDEX;
	}

	return type;
}

/// Creates the ActionQueue table in the database.
bool ActionQueue::create(const string &database)
{
	bool success = true;

	// The specified path must be a file
	if (SQLiteBase::check(database) == false)
	{
		return false;
	}

	SQLiteBase db(database);

	// Does ActionQueue exist ?
	if (db.executeSimpleStatement("SELECT * FROM ActionQueue LIMIT 1;") == false)
	{
#ifdef DEBUG
		cout << "ActionQueue::create: ActionQueue doesn't exist" << endl;
#endif
		// Create the table
		if (db.executeSimpleStatement("CREATE TABLE ActionQueue (QueueId VARCHAR(255), \
			Url VARCHAR(255), Type VARCHAR(255), Date INTEGER, Info TEXT, \
			PRIMARY KEY(QueueId, Url));") == false)
		{
			success = false;
		}
	}

	return success;
}

/// Pushes an item.
bool ActionQueue::pushItem(ActionType type, const DocumentInfo &docInfo)
{
	string url(docInfo.getLocation());
	string info("caption=");
	bool update = false, success = false;

	// Is there already an item for this URL ?
	SQLiteResults *results = executeStatement("SELECT Url FROM ActionQueue \
		WHERE QueueId='%q' AND Url='%q'",
		m_queueId.c_str(), Url::escapeUrl(url).c_str());
	if (results != NULL)
	{
		SQLiteRow *row = results->nextRow();
		if (row != NULL)
		{
#ifdef DEBUG
			cout << "ActionQueue::pushItem: item "
				<< Url::unescapeUrl(row->getColumn(0)) << " exists" << endl;
#endif
			update = true;

			delete row;
		}

		delete results;
	}

	// Serialize DocumentInfo
	info += docInfo.getTitle();
	info += "\n";
	info += "url=";
	info += url;
	info += "\n";
	info += "type=";
	info += docInfo.getType();
	info += "\n";
	info += "language=";
	info += docInfo.getLanguage();
	info += "\n";
	info += "modtime=";
	info += docInfo.getTimestamp();
	info += "\n";
	info += "size=";
	char sizeStr[64];
	snprintf(sizeStr, 64, "%ld", docInfo.getSize());
	info += sizeStr;
	info += "\n";

	if (update == false)
	{
		results = executeStatement("INSERT INTO ActionQueue \
			VALUES('%q', '%q', '%q', '%d', '%q');",
			m_queueId.c_str(), Url::escapeUrl(url).c_str(),
			typeToText(type).c_str(), time(NULL), Url::escapeUrl(info).c_str());
	}
	else
	{
		results = executeStatement("UPDATE ActionQueue \
			SET Type='%q', Date='%d', Info='%q' WHERE \
			QueueId='%q' AND Url='%q';",
			typeToText(type).c_str(), time(NULL), Url::escapeUrl(info).c_str(),
			m_queueId.c_str(), Url::escapeUrl(url).c_str());
	}
	if (results != NULL)
	{
#ifdef DEBUG
		cout << "ActionQueue::pushItem: queue " << m_queueId
			<< ": " << type << " on " << url << ", " << update << endl;
#endif
		success = true;

		delete results;
	}

	return success;
}

/// Pops and deletes the oldest item.
bool ActionQueue::popItem(ActionType &type, DocumentInfo &docInfo)
{
	string url;
	bool success = false;

	if (getOldestItem(type, docInfo) == false)
	{
		return false;
	}
	url = docInfo.getLocation();
#ifdef DEBUG
	cout << "ActionQueue::popItem: queue " << m_queueId
		<< ": " << type << " on " << url << endl;
#endif

	// Delete from ActionQueue
	SQLiteResults *results = executeStatement("DELETE FROM ActionQueue \
		WHERE QueueId='%q' AND Url='%q';",
		m_queueId.c_str(), Url::escapeUrl(url).c_str());
	if (results != NULL)
	{
		success = true;
		delete results;
	}

	return success;
}

bool ActionQueue::getOldestItem(ActionType &type, DocumentInfo &docInfo) const
{
	bool success = false;

	SQLiteResults *results = executeStatement("SELECT Url, Type, Info FROM ActionQueue \
		WHERE QueueId='%q' ORDER BY Date DESC LIMIT 1",
		m_queueId.c_str());
	if (results != NULL)
	{
		SQLiteRow *row = results->nextRow();
		if (row != NULL)
		{
			string url(Url::unescapeUrl(row->getColumn(0)));
			type = textToType(row->getColumn(1));
			string info(Url::unescapeUrl(row->getColumn(2)));
			success = true;
#ifdef DEBUG
			cout << "ActionQueue::getOldestItem: " << info << endl;
#endif

			// Deserialize DocumentInfo
			docInfo.setTitle(StringManip::extractField(info, "caption=", "\n"));
			docInfo.setLocation(url);
			docInfo.setType(StringManip::extractField(info, "type=", "\n"));
			docInfo.setLanguage(StringManip::extractField(info, "language=", "\n"));
			docInfo.setTimestamp(StringManip::extractField(info, "modtime=", "\n"));
			string bytesSize(StringManip::extractField(info, "size=", "\n"));
			if (bytesSize.empty() == false)
			{
				docInfo.setSize((off_t )atol(bytesSize.c_str()));
			}
#ifdef DEBUG
			cout << "ActionQueue::getOldestItem: " << docInfo.getTitle() << ", "
				<< docInfo.getLocation() << ", "
				<< docInfo.getType() << ", "
				<< docInfo.getLanguage() << ", "
				<< docInfo.getTimestamp() << ", "
				<< docInfo.getSize() << endl;
#endif

			delete row;
		}

		delete results;
	}

	return success;
}

/// Expires items older than the given date.
bool ActionQueue::expireItems(time_t expiryDate)
{
	bool success = false;

	SQLiteResults *results = executeStatement("DELETE FROM ActionQueue \
		WHERE QueueId='%q' AND Date<'%d';",
		m_queueId.c_str(), expiryDate);
	if (results != NULL)
	{
		success = true;
		delete results;
	}

	return success;
}
