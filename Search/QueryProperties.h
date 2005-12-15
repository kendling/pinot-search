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

#ifndef _QUERY_PROPERTIES_H
#define _QUERY_PROPERTIES_H

#include <string>

#include "Result.h"

using namespace std;

class QueryProperties
{
	public:
		QueryProperties();
		QueryProperties(string name, string andWords, string phrase, string anyWords, string notWords);
		QueryProperties(const QueryProperties &other);
		~QueryProperties();

		QueryProperties &operator=(const QueryProperties &other);
		bool operator==(const QueryProperties &other) const;
		bool operator<(const QueryProperties &other) const;

		/// Sets the name.
		void setName(const string &name);
		/// Gets the name.
		string getName(void) const;

		/// Sets AND words.
		void setAndWords(const string &words);
		/// Gets AND words.
		string getAndWords(void) const;

		/// Sets phrase query.
		void setPhrase(const string &phrase);
		/// Gets phrase query.
		string getPhrase(void) const;

		/// Sets ANY words.
		void setAnyWords(const string &words);
		/// Gets ANY words.
		string getAnyWords(void) const;

		/// Sets NOT words.
		void setNotWords(const string &words);
		/// Gets NOT words.
		string getNotWords(void) const;

		/// Sets the query's language.
		void setLanguage(const string &language);
		/// Gets the query's language.
		string getLanguage(void) const;

		/// Sets host filter.
		void setHostFilter(const string &filter);
		/// Gets host filter.
		string getHostFilter(void) const;

		/// Sets file filter.
		void setFileFilter(const string &filter);
		/// Gets file filter.
		string getFileFilter(void) const;

		/// Sets label filter.
		void setLabelFilter(const string &filter);
		/// Gets label filter.
		string getLabelFilter(void) const;

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

		/// Returns a displayable representation of this query's properties.
		string toString(bool forPresentation = true) const;

	protected:
		string m_name;
		string m_andWords;
		string m_phrase;
		string m_anyWords;
		string m_notWords;
		string m_language;
		string m_hostFilter;
		string m_fileFilter;
		string m_labelFilter;
		unsigned int m_resultsCount;
		bool m_indexResults;
		string m_labelName;

};

#endif // _QUERY_PROPERTIES_H
