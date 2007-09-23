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

#ifndef _QUERY_PROPERTIES_H
#define _QUERY_PROPERTIES_H

#include <string>
#include <set>

using namespace std;

/// This represents a query.
class QueryProperties
{
	public:
		typedef enum { XAPIAN_QP = 0, XESAM_QL, XESAM_UL } QueryType;

		QueryProperties();
		QueryProperties(const string &name, const string &freeQuery,
			QueryType type = XAPIAN_QP);
		QueryProperties(const QueryProperties &other);
		~QueryProperties();

		QueryProperties &operator=(const QueryProperties &other);
		bool operator==(const QueryProperties &other) const;
		bool operator<(const QueryProperties &other) const;

		/// Sets the name.
		void setName(const string &name);
		/// Gets the name.
		string getName(void) const;

		/// Sets the type.
		void setType(QueryType type);
		/// Gets the type.
		QueryType getType(void) const;

		/// Sets the query string.
		void setFreeQuery(const string &freeQuery);
		/// Gets the query string.
		string getFreeQuery(bool withoutFilters = false) const;

		/// Gets the value of a specific filter.
		string getFilter(const string &filterStr);

		/// Sets the maximum number of results.
		void setMaximumResultsCount(unsigned int count);
		/// Gets the maximum number of results.
		unsigned int getMaximumResultsCount(void) const;

		/// Sets whether results should be indexed.
		void setIndexResults(bool index);
		/// Gets whether results should be indexed
		bool getIndexResults(void) const;

		/// Sets the name of the label to use for indexed documents.
		void setLabelName(const string &labelName);
		/// Gets the name of the label to use for indexed documents.
		string getLabelName(void) const;

		/// Returns the query's terms.
		void getTerms(set<string> &terms) const;

		/// Returns whether the query is empty.
		bool isEmpty() const;

	protected:
		string m_name;
		QueryType m_type;
		string m_freeQuery;
		string m_freeQueryWithoutFilters;
		unsigned int m_resultsCount;
		bool m_indexResults;
		string m_labelName;

		void removeFilters(void);

};

#endif // _QUERY_PROPERTIES_H
