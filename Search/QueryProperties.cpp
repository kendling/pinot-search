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

#include "Document.h"
#include "Languages.h"
#include "StringManip.h"
#include "Tokenizer.h"
#include "QueryProperties.h"

QueryProperties::QueryProperties() :
	m_resultsCount(10),
	m_indexResults(false)
{
}

QueryProperties::QueryProperties(const string &name, const string &freeQuery) :
	m_name(name),
	m_freeQuery(freeQuery),
	m_resultsCount(10),
	m_indexResults(false)
{
	extractFilters();
}

QueryProperties::QueryProperties(const QueryProperties &other) :
	m_name(other.m_name),
	m_freeQuery(other.m_freeQuery),
	m_language(other.m_language),
	m_hostFilter(other.m_hostFilter),
	m_fileFilter(other.m_fileFilter),
	m_resultsCount(other.m_resultsCount),
	m_indexResults(other.m_indexResults),
	m_labelName(other.m_labelName)
{
	extractFilters();
}

QueryProperties::~QueryProperties()
{
}

QueryProperties &QueryProperties::operator=(const QueryProperties &other)
{
	if (this != &other)
	{
		m_name = other.m_name;
		m_freeQuery = other.m_freeQuery;
		m_language = other.m_language;
		m_hostFilter = other.m_hostFilter;
		m_fileFilter = other.m_fileFilter;
		m_resultsCount = other.m_resultsCount;
		m_indexResults = other.m_indexResults;
		m_labelName = other.m_labelName;
		extractFilters();
	}

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

void QueryProperties::extractFilters(void)
{
	if (m_freeQuery.empty() == true)
	{
		m_language.clear();
		m_hostFilter.clear();
		m_fileFilter.clear();
		return;
	}

	string freeQuery = StringManip::replaceSubString(m_freeQuery, "\n", " ");

	m_language = StringManip::extractField(freeQuery, "language:", " )", true);
	if (m_language.empty() == true)
	{
		m_language = StringManip::extractField(freeQuery, "language:", "");
	}
	// FIXME: do the same for host and file at the very least
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

/// Sets the query string.
void QueryProperties::setFreeQuery(const string &freeQuery)
{
	m_freeQuery = freeQuery;
}

/// Gets the query string.
string QueryProperties::getFreeQuery(void) const
{
	return m_freeQuery;
}

/// Gets the query's language.
string QueryProperties::getLanguage(void) const
{
	return m_language;
}

/// Gets the query's host filter.
string QueryProperties::getHostFilter(void) const
{
	return m_hostFilter;
}

/// Gets the query's file filter.
string QueryProperties::getFileFilter(void) const
{
	return m_fileFilter;
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

/// Returns the query's terms.
void QueryProperties::getTerms(set<string> &terms) const
{
	Document doc;

	doc.setData(m_freeQuery.c_str(), m_freeQuery.length());
	Tokenizer tokens(&doc);

	terms.clear();

	string token;
	while (tokens.nextToken(token) == true)
	{
		terms.insert(token);
	}
}
