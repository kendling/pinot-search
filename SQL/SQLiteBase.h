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

#ifndef _SQLITE_BASE_H
#define _SQLITE_BASE_H

#include <string>
#include <vector>

#include <sqlite3.h>

#include "SQLDB.h"

/// A row in a SQLite table.
class SQLiteRow : public SQLRow
{
	public:
		SQLiteRow(const std::vector<std::string> &rowColumns, unsigned int nColumns);
		virtual ~SQLiteRow();

		virtual std::string getColumn(unsigned int nColumn) const;

	protected:
		std::vector<std::string> m_columns;

};

/// Results extracted from a SQLite database.
class SQLiteResults : public SQLResults
{
	public:
		SQLiteResults(char **results, unsigned long nRows, unsigned int nColumns);
		virtual ~SQLiteResults();

		virtual std::string getColumnName(unsigned int nColumn) const;

		virtual SQLRow *nextRow(void);

	protected:
		char **m_results;

	private:
		SQLiteResults(const SQLiteResults &other);
		SQLiteResults &operator=(const SQLiteResults &other);

};

/// Simple C++ wrapper around the SQLite API.
class SQLiteBase : public SQLDB
{
	public:
		SQLiteBase(const std::string &databaseName, bool onDemand = true);
		virtual ~SQLiteBase();

		static bool check(const std::string &databaseName);

		virtual bool isOpen(void) const;

		virtual bool alterTable(const std::string &tableName,
			const std::string &columns,
			const std::string &newDefinition);

		virtual bool executeSimpleStatement(const std::string &sql);

		virtual SQLResults *executeStatement(const char *sqlFormat, ...);

	protected:
		bool m_onDemand;
		sqlite3 *m_pDatabase;

		void open(void);
		void close(void);

	private:
		SQLiteBase(const SQLiteBase &other);
		SQLiteBase &operator=(const SQLiteBase &other);

};

#endif // _SQLITE_BASE_H
