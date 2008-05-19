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
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <unistd.h>
#include <algorithm>
#include <iostream>

#include "SQLiteBase.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;

static int busyHandler(void *pData, int lockNum)
{
	// Try again
	return 1;
}

SQLiteRow::SQLiteRow(const vector<string> &rowColumns, unsigned int nColumns) :
	SQLRow(nColumns)
{
	if (rowColumns.empty() == false)
	{
		m_columns.reserve(rowColumns.size());
#if 0
		// FIXME: why does this segfault in string::assign() ?
		copy(rowColumns.begin(), rowColumns.end(), m_columns.begin());
#else
		for (vector<string>::const_iterator colIter = rowColumns.begin(); colIter != rowColumns.end(); ++colIter)
		{
			m_columns.push_back(*colIter);
		}
#endif
	}
}

SQLiteRow::~SQLiteRow()
{
}

string SQLiteRow::getColumn(unsigned int nColumn) const
{
	if (nColumn < m_nColumns)
	{
		vector<string>::const_iterator colIter = m_columns.begin();
		for (int i = 0; (i < m_nColumns) && (colIter != m_columns.end()); ++i)
		{
			if (i == nColumn)
			{
				string column(*colIter);

				return column;
			}
			++colIter;
		}
	}

	return "";
}

SQLiteResults::SQLiteResults(char **results, unsigned long nRows, unsigned int nColumns) :
	SQLResults(nRows, nColumns),
	m_results(results)
{
	// Check we actually have results
	if ((m_results == NULL) ||
		(m_nRows <= 0))
	{
		m_nRows = m_nCurrentRow = 0;
		m_nColumns = 0;
	}
}

SQLiteResults::~SQLiteResults()
{
	sqlite3_free_table(m_results);
}

string SQLiteResults::getColumnName(unsigned int nColumn) const
{
	if (nColumn < m_nColumns)
	{
		return m_results[nColumn];
	}

	return "";
}

SQLRow *SQLiteResults::nextRow(void)
{
	if ((m_nCurrentRow < 0) ||
		(m_nCurrentRow >= m_nRows))
	{
		return NULL;
	}

	// The very first row holds the column names
	unsigned long firstIndex = (m_nCurrentRow  + 1) * m_nColumns;
	unsigned long lastIndex = firstIndex + m_nColumns - 1;
	vector<string> rowColumns;

	for (unsigned long i = firstIndex; i <= lastIndex; ++i)
	{
		if (m_results[i] == NULL)
		{
			rowColumns.push_back("");
		}
		else
		{
			rowColumns.push_back(m_results[i]);
		}
	}
	++m_nCurrentRow;

	return new SQLiteRow(rowColumns, m_nColumns);
}

SQLiteBase::SQLiteBase(const string &databaseName, bool onDemand) :
	SQLDB(databaseName),
	m_onDemand(onDemand),
	m_pDatabase(NULL)
{
	if (m_onDemand == false)
	{
		open();
	}
}

SQLiteBase::~SQLiteBase()
{
	if (m_onDemand == false)
	{
		close();
	}
}

bool SQLiteBase::check(const string &databaseName)
{
	struct stat dbStat;

	// The specified path must be a file
	if ((stat(databaseName.c_str(), &dbStat) != -1) &&
		(!S_ISREG(dbStat.st_mode)))
	{
		// It exists, but it's not a file as expected
		cerr << databaseName << " is not a file" << endl;
		return false;
	}

	return true;
}

void SQLiteBase::open(void)
{
	// Open the new database
	if (sqlite3_open(m_databaseName.c_str(), &m_pDatabase) != SQLITE_OK)
	{
		// An handle is returned even when an error occurs !
		if (m_pDatabase != NULL)
		{
			cerr << sqlite3_errmsg(m_pDatabase) << endl;
			close();
			m_pDatabase = NULL;
		}
	}
	else if (m_pDatabase == NULL)
	{
		cerr << "Couldn't open " << m_databaseName << endl;
	}
	else
	{
		// Set up a busy handler
		sqlite3_busy_handler(m_pDatabase, busyHandler, NULL);
	}
}

void SQLiteBase::close(void)
{
	if (m_pDatabase != NULL)
	{
		sqlite3_close(m_pDatabase);
	}
}

bool SQLiteBase::isOpen(void) const
{
	if (m_pDatabase == NULL)
	{
		return false;
	}

	return true;
}

bool SQLiteBase::alterTable(const string &tableName,
	const string &columns, const string &newDefinition)
{
	if ((tableName.empty() == true) ||
		(columns.empty() == true) ||
		(newDefinition.empty() == true))
	{
		return false;
	}

	string sql("BEGIN TRANSACTION; CREATE TEMPORARY TABLE ");
	sql += tableName;
	sql += "_backup (";
	sql += columns;
	sql += "); INSERT INTO ";
	sql += tableName;
	sql += "_backup SELECT ";
	sql += columns;
	sql += " FROM ";
	sql += tableName;
	sql += "; DROP TABLE ";
	sql += tableName;
	sql += "; CREATE TABLE ";
	sql += tableName;
	sql += " (";
	sql += newDefinition;
	sql += "); INSERT INTO ";
	sql += tableName;
	sql += " SELECT ";
	sql += columns;
	sql += " FROM ";
	sql += tableName;
	sql += "_backup; DROP TABLE ";
	sql += tableName;
	sql += "_backup; COMMIT;";
#ifdef DEBUG
	cout << "SQLiteBase::alterTable: " << sql << endl;
#endif

	return executeSimpleStatement(sql);
}

bool SQLiteBase::executeSimpleStatement(const string &sql)
{
	char *errMsg = NULL;
	bool success = true;

	if (sql.empty() == true)
	{
		return false;
	}

	if (m_onDemand == true)
	{
		open();
	}
	if (m_pDatabase == NULL)
	{
		return false;
	}

	if (sqlite3_exec(m_pDatabase,
		sql.c_str(), 
		NULL, NULL, // No callback
		&errMsg) != SQLITE_OK)
	{
		if (errMsg != NULL)
		{
			cerr << "Statement <" << sql << "> failed: " << errMsg << endl;

			sqlite3_free(errMsg);
		}

		success = false;
	}
	if (m_onDemand == true)
	{
		close();
	}

	return success;
}

SQLResults *SQLiteBase::executeStatement(const char *sqlFormat, ...)
{
	SQLiteResults *pResults = NULL;
#ifdef _USE_VSNPRINTF
	char stringBuff[2048];
#endif
	va_list ap;

	if (sqlFormat == NULL)
	{
		return NULL;
	}

	if (m_onDemand == true)
	{
		open();
	}
	if (m_pDatabase == NULL)
	{
		return NULL;
	}

	va_start(ap, sqlFormat);
#ifdef _USE_VSNPRINTF
	int numChars = vsnprintf(stringBuff, 2048, sqlFormat, ap);
	if (numChars <= 0)
	{
#ifdef DEBUG
		cout << "SQLiteBase::executeStatement: couldn't format statement" << endl;
#endif
		if (m_onDemand == true)
		{
			close();
		}
		return NULL;
	}
	if (numChars >= 2048)
	{
		// Not enough space
#ifdef DEBUG
		cout << "SQLiteBase::executeStatement: not enough space (" << numChars << ")" << endl;
#endif
		if (m_onDemand == true)
		{
			close();
		}
		return NULL;
	}
	stringBuff[numChars] = '\0';
#else
	char *stringBuff = sqlite3_vmprintf(sqlFormat, ap);
	if (stringBuff == NULL)
	{
#ifdef DEBUG
		cout << "SQLiteBase::executeStatement: couldn't format statement" << endl;
#endif
		if (m_onDemand == true)
		{
			close();
		}
		return NULL;
	}
#endif

	char **results;
	char *errMsg;
	int nRows, nColumns;
	if (sqlite3_get_table(m_pDatabase,
			stringBuff,
			&results,
			&nRows,
			&nColumns,
			&errMsg) != SQLITE_OK)
	{
		if (errMsg != NULL)
		{
			cerr << "Statement <" << stringBuff << "> failed: " << errMsg << endl;

			sqlite3_free(errMsg);
		}
	}
	else
	{
		pResults = new SQLiteResults(results, (unsigned long)nRows, (unsigned int)nColumns);
	}
	va_end(ap);
#ifndef _USE_VSNPRINTF
	sqlite3_free(stringBuff);
#endif
	if (m_onDemand == true)
	{
		close();
	}

	return pResults;
}
