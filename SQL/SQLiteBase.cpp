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
#include <stdarg.h>
#include <unistd.h>
#include <algorithm>
#include <iostream>

#include "SQLiteBase.h"

using std::cout;
using std::endl;

SQLiteRow::SQLiteRow(const vector<string> &rowColumns, int nColumns) :
	m_nColumns(nColumns)
{
	if (rowColumns.empty() == true)
	{
		m_nColumns = 0;
	}
	else
	{
#ifdef DEBUG
		cout << "SQLiteRow::SQLiteRow: " << rowColumns.size() << " columns" << endl;
#endif
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

int SQLiteRow::getColumnsCount(void) const
{
	return m_nColumns;
}

string SQLiteRow::getColumn(int nColumn) const
{
	if (nColumn < m_nColumns)
	{
		vector<string>::const_iterator colIter = m_columns.begin();
		for (unsigned int i = 0; (i < m_nColumns) && (colIter != m_columns.end()); ++i)
		{
			if (i == nColumn)
			{
				string column = *colIter;
				return column;
			}
			++colIter;
		}
	}

	return "";
}

SQLiteResults::SQLiteResults(char **results, int nRows, int nColumns) :
	m_results(results),
	m_nRows(nRows),
	m_nColumns(nColumns),
	m_nCurrentRow(0)
{
	// Check we actually have results
	if ((m_results == NULL) ||
		(m_nRows <= 0))
	{
		m_nRows = m_nColumns = m_nCurrentRow = 0;
	}
}

SQLiteResults::~SQLiteResults()
{
	sqlite3_free_table(m_results);
}

bool SQLiteResults::hasMoreRows(void) const
{
	if ((m_nCurrentRow >= 0) &&
		(m_nCurrentRow < m_nRows))
	{
		return true;
	}

	return false;
}

string SQLiteResults::getColumnName(int nColumn) const
{
	if (nColumn < m_nColumns)
	{
		return m_results[nColumn];
	}

	return "";
}

SQLiteRow *SQLiteResults::nextRow(void)
{
	if ((m_nCurrentRow < 0) ||
		(m_nCurrentRow >= m_nRows))
	{
		return NULL;
	}

	// The very first row holds the column names
	unsigned int firstIndex = (m_nCurrentRow  + 1) * m_nColumns;
	unsigned int lastIndex = firstIndex + m_nColumns - 1;
	vector<string> rowColumns;

	for (unsigned int i = firstIndex; i <= lastIndex; ++i)
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

bool SQLiteResults::reset(void)
{
	m_nCurrentRow = 0;

	return true;
}

SQLiteBase::SQLiteBase(const string &database) :
	m_databaseName("")
{
	m_databaseName = database;
}

SQLiteBase::~SQLiteBase()
{
}

bool SQLiteBase::check(const string &database)
{
	struct stat dbStat;

	// The specified path must be a file
	if ((stat(database.c_str(), &dbStat) != -1) &&
		(!S_ISREG(dbStat.st_mode)))
	{
		// It exists, but it's not a file as expected
		cerr << "SQLiteBase::check: " << database << " is not a file" << endl;
		return false;
	}

	return true;
}

sqlite3 *SQLiteBase::open(const string &database) const
{
	sqlite3 *pDatabase = NULL;

	// Open the new database
	if ((sqlite3_open(database.c_str(), &pDatabase) != SQLITE_OK) ||
		(pDatabase == NULL))
	{
		cerr << "SQLiteBase::open: couldn't open " << database << endl;
		pDatabase = NULL;
	}
#ifdef DEBUG
	else cout << "SQLiteBase::open: opened " << database << endl;
#endif

	return pDatabase;
}

void SQLiteBase::close(sqlite3 *pDatabase) const
{
	if (pDatabase != NULL)
	{
#ifdef DEBUG
		cout << "SQLiteBase::close: changed " << sqlite3_total_changes(pDatabase) << " rows" << endl;
#endif
		sqlite3_close(pDatabase);
#ifdef DEBUG
		cout << "SQLiteBase::close: closed database" << endl;
#endif
	}
}

bool SQLiteBase::executeSimpleStatement(const string &sql) const
{
	char *errMsg = NULL;
	bool success = true;

	if (sql.empty() == true)
	{
		return false;
	}

	sqlite3 *pDatabase = open(m_databaseName);
	if (pDatabase == NULL)
	{
		return false;
	}

	if (sqlite3_exec(pDatabase,
		sql.c_str(), 
		NULL, NULL, // No callback
		&errMsg) != SQLITE_OK)
	{
		if (errMsg != NULL)
		{
			cerr << "SQLiteBase::executeSimpleStatement: statement <" << sql << "> failed: " << errMsg << endl;

			sqlite3_free(errMsg);
		}

		success = false;
	}
	close(pDatabase);

	return success;
}

SQLiteResults *SQLiteBase::executeStatement(const char *sqlFormat, ...) const
{
	SQLiteResults *pResults = NULL;
	const char *pzTail = NULL;
#ifdef _USE_VSNPRINTF
	char stringBuff[2048];
#endif
	va_list ap;

	if (sqlFormat == NULL)
	{
		return NULL;
	}

	sqlite3 *pDatabase = open(m_databaseName);
	if (pDatabase == NULL)
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
		close(pDatabase);
		return NULL;
	}
	if (numChars >= 2048)
	{
		// Not enough space
#ifdef DEBUG
		cout << "SQLiteBase::executeStatement: not enough space (" << numChars << ")" << endl;
#endif
		close(pDatabase);
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
		close(pDatabase);
		return NULL;
	}
#endif

	char **results;
	char *errMsg;
	int nRows, nColumns;
	if (sqlite3_get_table(pDatabase,
			stringBuff,
			&results,
			&nRows,
			&nColumns,
			&errMsg) != SQLITE_OK)
	{
		if (errMsg != NULL)
		{
			cerr << "SQLiteBase::executeStatement: statement <" << stringBuff << "> failed: " << errMsg << endl;

			sqlite3_free(errMsg);
		}
	}
	else
	{
		pResults = new SQLiteResults(results, nRows, nColumns);
	}
	va_end(ap);
#ifndef _USE_VSNPRINTF
	sqlite3_free(stringBuff);
#endif
	close(pDatabase);

	return pResults;
}
