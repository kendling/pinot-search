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

	string yyyymmddMin(TimeConverter::toYYYYMMDDString(minDay, minMonth, minYear));
	string yyyymmddMax(TimeConverter::toYYYYMMDDString(maxDay, maxMonth, maxYear));
	time_t startTime = TimeConverter::fromYYYYMMDDString(yyyymmddMin);
	time_t endTime = TimeConverter::fromYYYYMMDDString(yyyymmddMax);
  
	double diffTime = difftime(endTime, startTime);
	if (diffTime > 0)
	{
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
	bool bStatus = false;

	if (pIndex != NULL)
	{
		try
		{
			// Start an enquire session on the database
			Xapian::Enquire enquire(*pIndex);

			// Give the query object to the enquire session
			enquire.set_query(query);

			// Get the top results of the query
			Xapian::MSet matches = enquire.get_mset(0, m_maxResultsCount);
			if (matches.empty() == false)
			{
				multimap<Xapian::weight, string> queryTerms;
				vector<string> seedTerms;
				Xapian::weight maxWeight = matches.get_max_attained();

				// Sort query terms by weight
				for (Xapian::TermIterator termIter = query.get_terms_begin();
					termIter != query.get_terms_end(); ++termIter)
				{
					string termName(*termIter);
					Xapian::weight termWeight = matches.get_termweight(termName);

					if (termWeight > 0)
					{
						queryTerms.insert(pair<Xapian::weight, string>(maxWeight - termWeight, termName));
#ifdef DEBUG
						cout << "XapianEngine::queryDatabase: term " << termName
							<< " has weight " << termWeight << "/" << maxWeight << endl;
#endif
					}
				}

				for (multimap<Xapian::weight, string>::iterator weightIter = queryTerms.begin();
					weightIter != queryTerms.end(); ++weightIter)
				{
					seedTerms.push_back(weightIter->second);
				}

				// Get the results
#ifdef DEBUG
				cout << "XapianEngine::queryDatabase: " << matches.get_matches_estimated() << "/" << m_maxResultsCount << " results found" << endl;
#endif
				for (Xapian::MSetIterator mIter = matches.begin(); mIter != matches.end(); ++mIter)
				{
					Xapian::docid docId = *mIter;
					Xapian::Document doc(mIter.get_document());
					AbstractGenerator abstractGen(pIndex, 50);
					string record = doc.get_data();

					// Get the title
					string title = StringManip::extractField(record, "caption=", "\n");
#ifdef DEBUG
					cout << "XapianEngine::queryDatabase: found omindex title " << title << endl;
#endif
					// Get the URL
					string url = StringManip::extractField(record, "url=", "\n");
					if (url.empty() == true)
					{
						// Hmmm this shouldn't be empty...
						// Use this instead, even though the document isn't cached in the index
						url = XapianDatabase::buildUrl(m_databaseName, *mIter);
					}
					else
					{
#ifdef DEBUG
						cout << "XapianEngine::queryDatabase: found omindex URL " << url << endl;
#endif
						url = Url::canonicalizeUrl(url);
					}

					// Get the type
					string type = StringManip::extractField(record, "type=", "\n");
					// ...and the language, if available
					string language = StringManip::extractField(record, "language=", "\n");

					// Generate an abstract based on the query's terms
					string summary = abstractGen.generateAbstract(docId, seedTerms);

					// Add this result
					Result thisResult(url, title, summary, language, (float)mIter.get_percent());
					m_resultsList.push_back(thisResult);
				}
			}

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

			bStatus = true;
		}
		catch (const Xapian::Error &error)
		{
			cerr << "XapianEngine::queryDatabase: " << error.get_type() << ": " << error.get_msg() << endl;
		}
	}

	return bStatus;
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
			cout << "XapianEngine::runQuery: query terms are " << endl;
			for (Xapian::TermIterator termIter = fullQuery.get_terms_begin();
				termIter != fullQuery.get_terms_end(); ++termIter)
			{
				cout << " " << *termIter << endl;
			}
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

/// Returns the results for the previous query.
const vector<Result> &XapianEngine::getResults(void) const
{
	return m_resultsList;
}

