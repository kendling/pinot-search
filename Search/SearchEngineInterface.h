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

#ifndef _SEARCHENGINE_INTERFACE_H
#define _SEARCHENGINE_INTERFACE_H

#include <time.h>
#include <string>
#include <vector>

#include "QueryProperties.h"
#include "Result.h"

using namespace std;

/**
  * Interface implemented by search engines.
  */
class SearchEngineInterface
{
	public:
		virtual ~SearchEngineInterface();

		/// Indicates if the search engine is available in several languages/countries.
		virtual bool isInternational(void) const;

		/// Sets the search engine's key, if applicable.
		virtual void setKey(const string &key);

		/// Sets the number of calls issued since start time.
		virtual void setCallsCount(unsigned int count, time_t startTime);

		/// Sets the maximum number of results to return.
		virtual void setMaxResultsCount(unsigned int count);

		/// Runs a query; true if success.
		virtual bool runQuery(QueryProperties& queryProps) = 0;

		/// Returns the results for the previous query.
		virtual const vector<Result> &getResults(void) const;

		/// Returns the charset for the previous query's results.
		virtual string getResultsCharset(void) const;

	protected:
		string m_key;
		unsigned int m_callsCount;
		time_t m_startTime;
		unsigned int m_maxResultsCount;
		string m_hostFilter;
		string m_fileFilter;
		vector<Result> m_resultsList;
		string m_charset;

		SearchEngineInterface();

		void setHostNameFilter(const string &filter);

		void setFileNameFilter(const string &filter);

		virtual bool processResult(string &resultUrl);

};

#endif // _SEARCHENGINE_INTERFACE_H
