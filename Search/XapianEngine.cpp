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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>

#include "Languages.h"
#include "StringManip.h"
#include "TimeConverter.h"
#include "Url.h"
#include "XapianDatabaseFactory.h"
#include "AbstractGenerator.h"
#include "XapianEngine.h"

using std::string;
using std::multimap;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;
using std::inserter;

// The following function was lifted from Xapian's xapian-applications/omega/date.cc
// where it's called last_day(), and prettified slightly
static int lastDay(int year, int month)
{
	static const int lastDays[13] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	if (month != 2)
	{
		return lastDays[month];
	}

	// Good until 2100
	return 28 + (year % 4 == 0);
}

static void checkFilter(const string &freeQuery, string::size_type filterValueStart,
	bool &escapeValue, bool &hashValue)
{
	string filterName;
	string::size_type filterNameStart = freeQuery.rfind(' ', filterValueStart);

	escapeValue = hashValue = false;

	if (filterNameStart == string::npos)
	{
		filterName = freeQuery.substr(0, filterValueStart);
	}
	else
	{
		filterName = freeQuery.substr(filterNameStart + 1, filterValueStart - filterNameStart - 1);
	}
#ifdef DEBUG
	cout << "checkFilter: filter " << filterName << endl;
#endif

	// In XapianIndex, these are escaped and hashed
	if ((filterName == "file") ||
		(filterName =="dir") ||
		(filterName == "url"))
	{
		escapeValue = hashValue = true;
	}
	// except label which is only escaped
	else if (filterName == "label")
	{
		escapeValue = true;
	}
}

XapianEngine::XapianEngine(const string &database) :
	SearchEngineInterface()
{
	// If the database name ends with a slash, remove it
	if (database[database.length() - 1] == '/')
	{
		m_databaseName = database.substr(0, database.length() - 1);
	}
	else
	{
		m_databaseName = database;
	}

	// SearchEngineInterface members
	m_maxResultsCount = 10;
	m_resultsList.clear();
}

XapianEngine::~XapianEngine()
{
	m_resultsList.clear();
}

Xapian::Query XapianEngine::dateFilter(unsigned int minDay, unsigned int minMonth, unsigned int minYear,
	unsigned int maxDay, unsigned int maxMonth, unsigned int maxYear)
{
	// The following was lifted from Xapian's date_range_filter() in
	// xapian-applications/omega/date.cc and prettified slightly
	vector<Xapian::Query> v;
	char buf[10];

	snprintf(buf, 10, "D%04d%02d", minYear, minMonth);
	int d_last = lastDay(minYear, minMonth);
	int d_end = d_last;
	if (minYear == maxYear && minMonth == maxMonth && maxDay < d_last)
	{
		d_end = maxDay;
	}
	// Deal with any initial partial month
	if (minDay > 1 || d_end < d_last)
	{
		for ( ; minDay <= d_end ; minDay++)
		{
			snprintf(buf + 7, 3, "%02d", minDay);
			v.push_back(Xapian::Query(buf));
		}
	} else {
		buf[0] = 'M';
		v.push_back(Xapian::Query(buf));
	}
	
	if (minYear == maxYear && minMonth == maxMonth)
	{
		return Xapian::Query(Xapian::Query::OP_OR, v.begin(), v.end());
	}

	int m_last = (minYear < maxYear) ? 12 : maxMonth - 1;
	while (++minMonth <= m_last)
	{
		snprintf(buf + 5, 5, "%02d", minMonth);
		buf[0] = 'M';
		v.push_back(Xapian::Query(buf));
	}
	
	if (minYear < maxYear)
	{
		while (++minYear < maxYear)
		{
			snprintf(buf + 1, 9, "%04d", minYear);
			buf[0] = 'Y';
			v.push_back(Xapian::Query(buf));
		}
		snprintf(buf + 1, 9, "%04d", maxYear);
		buf[0] = 'M';
		for (minMonth = 1; minMonth < maxMonth; minMonth++)
		{
			snprintf(buf + 5, 5, "%02d", minMonth);
			v.push_back(Xapian::Query(buf));
		}
	}
	
	snprintf(buf + 5, 5, "%02d", maxMonth);

	// Deal with any final partial month
	if (maxDay < lastDay(maxYear, maxMonth))
	{
		buf[0] = 'D';
		for (minDay = 1 ; minDay <= maxDay; minDay++)
		{
			snprintf(buf + 7, 3, "%02d", minDay);
			v.push_back(Xapian::Query(buf));
		}
	}
	else
	{
		buf[0] = 'M';
		v.push_back(Xapian::Query(buf));
	}

	return Xapian::Query(Xapian::Query::OP_OR, v.begin(), v.end());
}

Xapian::Query XapianEngine::parseQuery(Xapian::Database *pIndex, const QueryProperties &queryProps,
	const string &stemLanguage, bool followOperators)
{
	string freeQuery(StringManip::replaceSubString(queryProps.getFreeQuery(), "\n", " "));
	Xapian::QueryParser parser;
	Xapian::Stem stemmer;
	unsigned int minDay, minMonth, minYear = 0;
	unsigned int maxDay, maxMonth, maxYear = 0;

	// Set things up
	if (stemLanguage.empty() == false)
	{
		stemmer = Xapian::Stem(StringManip::toLowerCase(stemLanguage));
		parser.set_stemming_strategy(Xapian::QueryParser::STEM_NONE);
	}
	else
	{
		parser.set_stemming_strategy(Xapian::QueryParser::STEM_ALL);
	}
	parser.set_stemmer(stemmer);
	if (followOperators == true)
	{
		parser.set_default_op(Xapian::Query::OP_AND);
	}
	else
	{
		parser.set_default_op(Xapian::Query::OP_OR);
	}
	if (pIndex != NULL)
	{
		// The database is required for wildcards
		parser.set_database(*pIndex);
	}
	// ...including prefixes
	// X prefixes should always include a colon
	parser.add_boolean_prefix("site", "H");
	parser.add_boolean_prefix("file", "P");
	parser.add_boolean_prefix("ext", "XEXT:");
	parser.add_boolean_prefix("title", "S");
	parser.add_boolean_prefix("url", "U");
	parser.add_boolean_prefix("dir", "XDIR:");
	parser.add_boolean_prefix("lang", "L");
	parser.add_boolean_prefix("type", "T");
	parser.add_boolean_prefix("label", "XLABEL:");

	// Do some pre-processing : look for filters with quoted values
	string::size_type escapedFilterEnd = 0;
	string::size_type escapedFilterStart = freeQuery.find(":\"");
	while ((escapedFilterStart != string::npos) &&
		(escapedFilterStart < freeQuery.length() - 2))
	{
		escapedFilterEnd = freeQuery.find("\"", escapedFilterStart + 2);
		if (escapedFilterEnd == string::npos)
		{
			break;
		}

		string filterValue = freeQuery.substr(escapedFilterStart + 2, escapedFilterEnd - escapedFilterStart - 2);
		if (filterValue.empty() == false)
		{
			string escapedValue(Url::escapeUrl(filterValue));
			bool escapeValue = false, hashValue = false;

			// The value should be escaped and length-limited as done at indexing time
			checkFilter(freeQuery, escapedFilterStart, escapeValue, hashValue);

			if (escapeValue == false)
			{
				// No escaping
				escapedValue = filterValue;
			}
			if (hashValue == true)
			{
				// Partially hash if necessary
				escapedValue = XapianDatabase::limitTermLength(escapedValue, true);
			}
			else
			{
				escapedValue = XapianDatabase::limitTermLength(escapedValue);
			}

			freeQuery.replace(escapedFilterStart + 1, escapedFilterEnd - escapedFilterStart,
				escapedValue);
			escapedFilterEnd = escapedFilterEnd + escapedValue.length() - filterValue.length();
		}
		else
		{
			// No value !
			freeQuery.replace(escapedFilterStart, escapedFilterEnd - escapedFilterStart + 1, ":");
			escapedFilterEnd -= 2;
		}
#ifdef DEBUG
		cout << "XapianEngine::parseQuery: replaced filter: " << freeQuery << endl;
#endif

		// Next
		escapedFilterStart = freeQuery.find(":\"", escapedFilterEnd);
	}

	// Activate all options and parse
	Xapian::Query parsedQuery = parser.parse_query(freeQuery,
		Xapian::QueryParser::FLAG_BOOLEAN|Xapian::QueryParser::FLAG_PHRASE|Xapian::QueryParser::FLAG_LOVEHATE|Xapian::QueryParser::FLAG_BOOLEAN_ANY_CASE|Xapian::QueryParser::FLAG_WILDCARD);
	// Apply a date range ?
	bool enableMin = queryProps.getMinimumDate(minDay, minMonth, minYear);
	bool enableMax = queryProps.getMaximumDate(maxDay, maxMonth, maxYear);
	if ((enableMin == false) && 
		(enableMax == false))
	{
		// No
		return parsedQuery;
	}

	// Anyone going as far back as Year 0 is taking the piss :-)
	if ((enableMin == false) ||
		(minYear == 0))
	{
		minDay = minMonth = 1;
		minYear = 1970;
	}
	// If the second date is older than the Epoch, the first date should be set too
	if ((enableMax == false) ||
		(maxYear == 0))
	{
		time_t nowTime = time(NULL);
		struct tm *timeTm = localtime(&nowTime);
		maxYear = timeTm->tm_year + 1900;
		maxMonth = timeTm->tm_mon + 1;
		maxDay = timeTm->tm_mday;
	}

	string yyyymmddMin(TimeConverter::toYYYYMMDDString(minYear, minMonth, minDay));
	string yyyymmddMax(TimeConverter::toYYYYMMDDString(maxYear, maxMonth, maxDay));
	time_t startTime = TimeConverter::fromYYYYMMDDString(yyyymmddMin);
	time_t endTime = TimeConverter::fromYYYYMMDDString(yyyymmddMax);
 
	double diffTime = difftime(endTime, startTime);
	if (diffTime > 0)
	{
#ifdef DEBUG
		cout << "XapianEngine::parseQuery: applied date range ("
			<< yyyymmddMax << " <= " << yyyymmddMin << ")" << endl;
#endif
		return Xapian::Query(Xapian::Query::OP_FILTER, parsedQuery,
			dateFilter(minDay, minMonth, minYear, maxDay, maxMonth, maxYear));
	}
#ifdef DEBUG
	else cout << "XapianEngine::parseQuery: date range is zero or bogus ("
		<< yyyymmddMax << " <= " << yyyymmddMin << ")" << endl;
#endif

	return parsedQuery;
}

/// Validates a query and extracts its terms.
bool XapianEngine::validateQuery(QueryProperties& queryProps, bool includePrefixed,
	vector<string> &terms)
{
	bool goodQuery = false;

	try
	{
		Xapian::Query fullQuery = parseQuery(NULL, queryProps, "", true);
		if (fullQuery.empty() == false)
		{
			for (Xapian::TermIterator termIter = fullQuery.get_terms_begin();
				termIter != fullQuery.get_terms_end(); ++termIter)
			{
				// Skip prefixed terms unless instructed otherwise
				if ((includePrefixed == true) ||
					(isupper((int)((*termIter)[0])) == 0))
				{
					terms.push_back(*termIter);
				}
			}

			goodQuery = true;
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "XapianEngine::validateQuery: " << error.get_type() << ": " << error.get_msg() << endl;
	}

	return goodQuery;
}

bool XapianEngine::queryDatabase(Xapian::Database *pIndex, Xapian::Query &query)
{
	bool completedQuery = false;

	if (pIndex == NULL)
	{
		return false;
	}

	// Start an enquire session on the database
	Xapian::Enquire enquire(*pIndex);

	try
	{
		AbstractGenerator abstractGen(pIndex, 50);
		vector<string> seedTerms;

		// Give the query object to the enquire session
		enquire.set_query(query);

		// Get the top results of the query
		Xapian::MSet matches = enquire.get_mset(0, m_maxResultsCount);
		if (matches.empty() == false)
		{
			// Get the results
#ifdef DEBUG
			cout << "XapianEngine::queryDatabase: " << matches.get_matches_estimated()
				<< "/" << m_maxResultsCount << " results found" << endl;
#endif
			for (Xapian::MSetIterator mIter = matches.begin(); mIter != matches.end(); ++mIter)
			{
				Xapian::docid docId = *mIter;
				Xapian::Document doc(mIter.get_document());

				// What terms did this document match ?
				seedTerms.clear();
				for (Xapian::TermIterator termIter = enquire.get_matching_terms_begin(docId);
					termIter != enquire.get_matching_terms_end(docId); ++termIter)
				{
					seedTerms.push_back(*termIter);
				}

				DocumentInfo thisResult;
				thisResult.setExtract(abstractGen.generateAbstract(docId, seedTerms));
				thisResult.setScore((float)mIter.get_percent());

#ifdef DEBUG
				cout << "XapianEngine::queryDatabase: found document ID " << docId << endl;
#endif
				XapianDatabase::recordToProps(doc.get_data(), &thisResult);

				string url(thisResult.getLocation());
				if (url.empty() == true)
				{
					// Hmmm this shouldn't be empty...
					// Use this instead, even though the document isn't cached in the index
					thisResult.setLocation(XapianDatabase::buildUrl(m_databaseName, docId));
				}

				// We don't know the index ID, just the document ID
				thisResult.setIsIndexed(0, docId);

				// Add this result
				m_resultsList.push_back(thisResult);
			}
		}

		completedQuery = true;
	}
	catch (const Xapian::Error &error)
	{
		cerr << "XapianEngine::queryDatabase: " << error.get_type() << ": " << error.get_msg() << endl;
	}

	try
	{
		m_expandTerms.clear();

		// Expand the query ?
		if (m_relevantDocuments.empty() == false)
		{
			Xapian::RSet relevantDocs;
			unsigned int count = 0;

			for (set<unsigned int>::const_iterator docIter = m_relevantDocuments.begin();
				docIter != m_relevantDocuments.end(); ++docIter)
			{
				relevantDocs.add_document(*docIter);
			}

			// Get 10 non-prefixed terms
			Xapian::ESet expandTerms = enquire.get_eset(20, relevantDocs);
			for (Xapian::ESetIterator termIter = expandTerms.begin();
				(termIter != expandTerms.end()) && (count < 10); ++termIter)
			{
				if (isupper((int)((*termIter)[0])) == 0)
				{
					m_expandTerms.insert(*termIter);
					++count;
				}
			}
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "XapianEngine::queryDatabase: " << error.get_type() << ": " << error.get_msg() << endl;
	}

	// Be tolerant of errors as long as we got some results
	if ((completedQuery == true) ||
		(m_resultsList.empty() == false))
	{
		return true;
	}

	return false;
}

//
// Implementation of SearchEngineInterface
//

/// Sets whether the query should be expanded.
bool XapianEngine::setQueryExpansion(set<unsigned int> &relevantDocuments)
{
	copy(relevantDocuments.begin(), relevantDocuments.end(),
		inserter(m_relevantDocuments, m_relevantDocuments.begin()));

	return true;
}

/// Runs a query; true if success.
bool XapianEngine::runQuery(QueryProperties& queryProps)
{
	// Clear the results list
	m_resultsList.clear();

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, true);
	if (pDatabase == NULL)
	{
		return false;
	}

	// Get the latest revision...
	pDatabase->reopen();
	Xapian::Database *pIndex = pDatabase->readLock();
	try
	{
		string stemLanguage;
		unsigned int searchStep = 1;
		bool followOperators = true;

		// Searches are run in this order :
		// 1. follow operators and don't stem terms
		// 2. if no results, follow operators and stem terms
		// 3. if no results, don't follow operators and don't stem terms
		// 4. if no results, don't follow operators and stem terms
		// Steps 2 and 4 depend on a language being defined for the query
		Xapian::Query fullQuery = parseQuery(pIndex, queryProps, "", followOperators);
		while (fullQuery.empty() == false)
		{
#ifdef DEBUG
			cout << "XapianEngine::runQuery: " << fullQuery.get_description() << endl;
#endif
			// Query the database
			if (queryDatabase(pIndex, fullQuery) == false)
			{
				break;
			}

			if (m_resultsList.empty() == true)
			{
				// The search did succeed but didn't return anything
				// Try the next step
				switch (++searchStep)
				{
					case 2:
						followOperators = true;
						stemLanguage = queryProps.getLanguage();
						if (stemLanguage.empty() == false)
						{
							break;
						}
						++searchStep;
					case 3:
						followOperators = false;
						stemLanguage.clear();
						break;
					case 4:
						followOperators = false;
						stemLanguage = queryProps.getLanguage();
						if (stemLanguage.empty() == false)
						{
							break;
						}
						++searchStep;
					default:
						pDatabase->unlock();
						return true;
				}

#ifdef DEBUG
				cout << "XapianEngine::runQuery: trying step " << searchStep << endl;
#endif
				fullQuery = parseQuery(pIndex, queryProps,
					Languages::toEnglish(stemLanguage), followOperators);
				continue;
			}

			pDatabase->unlock();
			return true;
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "XapianEngine::runQuery: " << error.get_type() << ": " << error.get_msg() << endl;
	}
	pDatabase->unlock();

	return false;
}
