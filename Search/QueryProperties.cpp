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

#include <iostream>
#include <algorithm>

#include "QueryProperties.h"

QueryProperties::QueryProperties()
{
	m_resultsCount = 10;
	m_indexResults = false;
}

QueryProperties::QueryProperties(string name, string andWords, string phrase, string anyWords, string notWords)
{
	m_name = name;
	m_andWords = andWords;
	m_phrase = phrase;
	m_anyWords = anyWords;
	m_notWords = notWords;
	m_resultsCount = 10;
	m_indexResults = false;
}

QueryProperties::QueryProperties(const QueryProperties &other) :
	m_name(other.m_name),
	m_andWords(other.m_andWords),
	m_phrase(other.m_phrase),
	m_anyWords(other.m_anyWords),
	m_notWords(other.m_notWords),
	m_language(other.m_language),
	m_hostName(other.m_hostName),
	m_fileName(other.m_fileName),
	m_resultsCount(other.m_resultsCount),
	m_indexResults(other.m_indexResults),
	m_labelName(other.m_labelName)
{
}

QueryProperties::~QueryProperties()
{
}

QueryProperties &QueryProperties::operator=(const QueryProperties &other)
{
	m_name = other.m_name;
	m_andWords = other.m_andWords;
	m_phrase = other.m_phrase;
	m_anyWords = other.m_anyWords;
	m_notWords = other.m_notWords;
	m_language = other.m_language;
	m_hostName = other.m_hostName;
	m_fileName = other.m_fileName;
	m_resultsCount = other.m_resultsCount;
	m_indexResults = other.m_indexResults;
	m_labelName = other.m_labelName;

	return *this;
}

bool QueryProperties::operator==(const QueryProperties &other) const
{
	if (m_name == other.m_name)
	{
		return true;
	}

	return false;
}

bool QueryProperties::operator<(const QueryProperties &other) const
{
	if (m_name < other.m_name)
	{
		return true;
	}

	return false;
}

/// Sets the name.
void QueryProperties::setName(const string &name)
{
	m_name = name;
}

/// Gets the name.
string QueryProperties::getName(void) const
{
	return m_name;
}

/// Sets AND words.
void QueryProperties::setAndWords(const string &words)
{
	m_andWords = words;
}

/// Gets AND words.
string QueryProperties::getAndWords(void) const
{
	return m_andWords;
}

/// Sets phrase query.
void QueryProperties::setPhrase(const string &phrase)
{
	m_phrase = phrase;
}

/// Gets phrase query.
string QueryProperties::getPhrase(void) const
{
	return m_phrase;
}

/// Sets ANY words.
void QueryProperties::setAnyWords(const string &words)
{
	m_anyWords = words;
}

/// Gets ANY words.
string QueryProperties::getAnyWords(void) const
{
	return m_anyWords;
}

/// Sets NOT words.
void QueryProperties::setNotWords(const string &words)
{
	m_notWords = words;
}

/// Gets NOT words.
string QueryProperties::getNotWords(void) const
{
	return m_notWords;
}

/// Sets the query's language.
void QueryProperties::setLanguage(const string &language)
{
	m_language = language;
}

/// Gets the query's language.
string QueryProperties::getLanguage(void) const
{
	return m_language;
}

/// Sets host name filter.
void QueryProperties::setHostNameFilter(const string &filter)
{
	m_hostName = filter;
}

/// Gets host name filter.
string QueryProperties::getHostNameFilter(void) const
{
	return 	m_hostName;
}

/// Sets file name filter.
void QueryProperties::setFileNameFilter(const string &filter)
{
	m_fileName = filter;
}

/// Gets file name filter.
string QueryProperties::getFileNameFilter(void) const
{
	return m_fileName;
}

/// Sets the maximum number of results.
void QueryProperties::setMaximumResultsCount(unsigned int count)
{
	m_resultsCount = count;
}

/// Gets the maximum number of results.
unsigned int QueryProperties::getMaximumResultsCount(void) const
{
	return m_resultsCount;
}

/// Sets whether results should be indexed.
void QueryProperties::setIndexResults(bool index)
{
	m_indexResults = index;
}

/// Gets whether results should be indexed
bool QueryProperties::getIndexResults(void) const
{
	return m_indexResults;
}

/// Sets the name of the label to use for indexed documents.
void QueryProperties::setLabelName(const string &labelName)
{
	m_labelName = labelName;
}

/// Gets the name of the label to use for indexed documents.
string QueryProperties::getLabelName(void) const
{
	return m_labelName;
}

/// Returns a displayable representation of this query's properties.
string QueryProperties::toString(bool forPresentation) const
{
	string queryString;
	if (m_andWords.empty() == false)
	{
		string tmp = m_andWords;
		replace(tmp.begin(), tmp.end(), ' ', '+');
		queryString += tmp;
	}
	if (m_anyWords.empty() == false)
	{
		if (forPresentation == true)
		{
			string tmp = m_anyWords;
			replace(tmp.begin(), tmp.end(), ' ', '|');
			queryString += " +(";
			queryString += tmp;
			queryString += ")";
		}
		else
		{
			string tmp = m_anyWords;
			// FIXME: is this good enough ?
			replace(tmp.begin(), tmp.end(), ' ', '+');
			if (queryString.empty() == false)
			{
				queryString += "+";
			}
			queryString += tmp;
		}
	}
	if (m_notWords.empty() == false)
	{
		string tmp = m_notWords;
		replace(tmp.begin(), tmp.end(), ' ', '-');
		if (queryString.empty() == false)
		{
			queryString += "-";
		}
		queryString += tmp;
		queryString += " ";
	}
	if (m_phrase.empty() == false)
	{
		string tmp = m_phrase;
		replace(tmp.begin(), tmp.end(), ' ', '+');
		if (queryString.empty() == false)
		{
			queryString += "+";
		}
		queryString += "\"";
		queryString += tmp;
		queryString += "\"";
	}
	if (forPresentation == true)
	{
		if (m_language.empty() == false)
		{
			queryString += " +L";
			queryString += m_language;
		}
		if (m_hostName.empty() == false)
		{
			queryString += " +H";
			queryString += m_hostName;
		}
		if (m_fileName.empty() == false)
		{
			queryString += " +F";
			queryString += m_fileName;
		}
	}

	return queryString;
}
