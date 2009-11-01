/*
 *  Copyright 2005-2009 Fabrice Colin
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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <unistd.h>
#include <algorithm>
#include <utility>
#include <iostream>

#include "SQLiteBase.h"

using std::clog;
using std::endl;
using std::string;
using std::vector;
using std::map;
using std::pair;

static int busyHandler(void *pData, int lockNum)
{
	// Try again
	return 1;
}

// A function object to delete statements with for_each()
struct DeleteStatementsFunc
{
	public:
		void operator()(map<string, sqlite3_stmt*>::value_type &p)
		{
			if (p.second != NULL)
			{
				sqlite3_finalize(p.second);
			}
		}
};

SQLiteRow::SQLiteRow(const vector<string> &rowColumns, unsigned int nColumns) :
	SQLRow(nColumns),
	m_pStatement(NULL)
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

SQLiteRow::SQLiteRow(sqlite3_stmt *pStatement, unsigned int nColumns) :
	SQLRow(nColumns),
	m_pStatement(pStatement)
{
}

SQLiteRow::~SQLiteRow()
{
}

string SQLiteRow::getColumn(unsigned int nColumn) const
{
	if (m_pStatement != NULL)
	{
		const unsigned char *pTextColumn = sqlite3_column_text(m_pStatement, nColumn);
		if (pTextColumn != NULL)
		{
			return (const char*)pTextColumn;
		}
#ifdef DEBUG
		clog << "SQLiteRow::getColumn: couldn't get column " << nColumn << endl;
#endif

		return "";
	}

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
	m_results(results),
	m_pStatement(NULL)
{
	// Check we actually have results
	if ((m_results == NULL) ||
		(m_nRows <= 0))
	{
		m_nRows = m_nCurrentRow = 0;
		m_nColumns = 0;
	}
}

SQLiteResults::SQLiteResults(sqlite3_stmt *pStatement) :
	SQLResults(0, sqlite3_column_count(pStatement)),
	m_results(NULL),
	m_pStatement(pStatement)
{
}

SQLiteResults::~SQLiteResults()
{
	if (m_results != NULL)
	{
		sqlite3_free_table(m_results);
	}
	if (m_pStatement != NULL)
	{
		rewind();
	}
}

bool SQLiteResults::hasMoreRows(void) const
{
	if (m_pStatement != NULL)
	{
		// There's no way to tell
		return true;
	}

	return SQLResults::hasMoreRows();
}

string SQLiteResults::getColumnName(unsigned int nColumn) const
{
	if (m_pStatement != NULL)
	{
		return sqlite3_column_name(m_pStatement, (int)nColumn);
	}

	if (nColumn < m_nColumns)
	{
		return m_results[nColumn];
	}

	return "";
}

SQLRow *SQLiteResults::nextRow(void)
{
	if (m_pStatement != NULL)
	{
		if (sqlite3_step(m_pStatement) == SQLITE_ROW)
		{
			++m_nCurrentRow;
			return new SQLiteRow(m_pStatement, m_nColumns);
		}
#ifdef DEBUG
		clog << "SQLiteResults::nextRow: no more row" << endl;
#endif

		return NULL;
	}

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

bool SQLiteResults::rewind(void)
{
	SQLResults::rewind();
	if (m_pStatement != NULL)
	{
		sqlite3_reset(m_pStatement);
	}

	return true;
}

SQLiteBase::SQLiteBase(const string &databaseName,
	bool readOnly, bool onDemand) :
	SQLDB(databaseName, readOnly),
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
		clog << databaseName << " is not a file" << endl;
		return false;
	}

	return true;
}

void SQLiteBase::open(void)
{
	int openFlags = SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE;

	if (m_readOnly == true)
	{
		openFlags = SQLITE_OPEN_READONLY;
	}

	// Open the new database
	if (sqlite3_open_v2(m_databaseName.c_str(), &m_pDatabase,
		openFlags, NULL) != SQLITE_OK)
	{
		// An handle is returned even when an error occurs !
		if (m_pDatabase != NULL)
		{
			clog << sqlite3_errmsg(m_pDatabase) << endl;
			close();
			m_pDatabase = NULL;
		}
	}
	else if (m_pDatabase == NULL)
	{
		clog << "Couldn't open " << m_databaseName << endl;
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
		for_each(m_statements.begin(), m_statements.end(),
			DeleteStatementsFunc());

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
	clog << "SQLiteBase::alterTable: " << sql << endl;
#endif

	return executeSimpleStatement(sql);
}

bool SQLiteBase::beginTransaction(void)
{
	if (m_onDemand == true)
	{
		// Not applicable
		return true;
	}

	return executeSimpleStatement("BEGIN TRANSACTION;");
}

bool SQLiteBase::endTransaction(void)
{
	if (m_onDemand == true)
	{
		// Not applicable
		return true;
	}

	return executeSimpleStatement("END TRANSACTION;");
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
			clog << m_databaseName << ": statement <" << sql << "> failed: " << errMsg << endl;

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
		clog << "SQLiteBase::executeStatement: couldn't format statement" << endl;
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
		clog << "SQLiteBase::executeStatement: not enough space (" << numChars << ")" << endl;
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
		clog << "SQLiteBase::executeStatement: couldn't format statement" << endl;
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
			clog << m_databaseName << ": statement <" << stringBuff << "> failed: " << errMsg << endl;

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

bool SQLiteBase::prepareStatement(const string &statementId,
	const string &sqlFormat)
{
	if ((sqlFormat.empty() == true) ||
		(m_onDemand == true) ||
		(m_pDatabase == NULL))
	{
		return false;
	}

	map<string, sqlite3_stmt*>::iterator statIter = m_statements.find(statementId);
	if (statIter != m_statements.end())
	{
#ifdef DEBUG
		clog << "SQLiteBase::prepareStatement: invalid statement ID " << statementId << endl;
#endif
		return false;
	}

	sqlite3_stmt *pStatement = NULL;
	if (sqlite3_prepare_v2(m_pDatabase,
		sqlFormat.c_str(),
		(int)sqlFormat.length(),
		&pStatement,
		NULL) == SQLITE_OK)
	{
		if (pStatement != NULL)
		{
#ifdef DEBUG
			clog << "SQLiteBase::prepareStatement: compiled statement ID " << statementId << endl;
#endif
			m_statements.insert(pair<string, sqlite3_stmt*>(statementId, pStatement));
			return true;
		}
	}

	clog << m_databaseName << ": failed to compile statement <" << sqlFormat << ">" << endl;

	return false;
}

SQLResults * SQLiteBase::executePreparedStatement(const string &statementId,
	const vector<string> &values)
{
	int paramIndex = 1;

	map<string, sqlite3_stmt*>::iterator statIter = m_statements.find(statementId);
	if (statIter == m_statements.end())
	{
#ifdef DEBUG
		clog << "SQLiteBase::executePreparedStatement: invalid statement ID " << statementId << endl;
#endif
		return NULL;
	}

	// Bind values
	// The left-most parameter's index is 1
	for (vector<string>::const_iterator valueIter = values.begin();
		valueIter != values.end(); ++valueIter, ++paramIndex)
	{
		int errorCode = sqlite3_bind_text(statIter->second, paramIndex,
			valueIter->c_str(), -1, SQLITE_TRANSIENT);
		if (errorCode != SQLITE_OK)
		{
			clog << m_databaseName << ": failed to bind parameter " << paramIndex
				<< " to statement ID " << statementId << ": error " << errorCode << endl;
			return NULL;
		}
#ifdef DEBUG
		clog << "SQLiteBase::executePreparedStatement: bound parameter " << paramIndex
			<< " size " << valueIter->length() << " to statement ID " << statementId << endl;
#endif
	}

#ifdef DEBUG
	clog << "SQLiteBase::executePreparedStatement: statement ID " << statementId << endl;
#endif
	return new SQLiteResults(statIter->second);
}

