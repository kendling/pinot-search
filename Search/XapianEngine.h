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

#ifndef _XAPIAN_ENGINE_H
#define _XAPIAN_ENGINE_H

#include <string>
#include <set>
#include <vector>

#include <xapian.h>

#include "SearchEngineInterface.h"
#include "DownloaderFactory.h"

class XapianEngine : public SearchEngineInterface
{
	public:
		XapianEngine(const std::string &database);
		virtual ~XapianEngine();

		/// Validates a query and extracts its terms.
		static bool validateQuery(QueryProperties& queryProps, bool includePrefixed,
			std::vector<std::string> &terms);

		/// Sets whether the query should be expanded.
		bool setQueryExpansion(std::set<unsigned int> &relevantDocuments);

		/// Runs a query; true if success.
		virtual bool runQuery(QueryProperties& queryProps);

		/// Returns the results for the previous query.
		virtual const std::vector<Result> &getResults(void) const;

	protected:
		std::string m_databaseName;
		std::set<unsigned int> m_relevantDocuments;

		bool queryDatabase(Xapian::Database *pIndex, Xapian::Query &query);

		static Xapian::Query parseQuery(Xapian::Database *pIndex, const QueryProperties &queryProps,
			const string &stemLanguage, bool followOperators);

	private:
		XapianEngine(const XapianEngine &other);
		XapianEngine &operator=(const XapianEngine &other);

};

#endif // _XAPIAN_ENGINE_H
