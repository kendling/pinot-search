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
#include "XapianQueryBuilder.h"
#include "XesamQLParser.h"
#include "XesamULParser.h"

using std::string;
using std::multimap;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;
using std::inserter;
using namespace Dijon;

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

class TimeValueRangeProcessor : public Xapian::ValueRangeProcessor
{
	public:
		TimeValueRangeProcessor(Xapian::valueno valno) : Xapian::ValueRangeProcessor(), m_valueNumber(valno) { }

		Xapian::valueno operator()(string &begin, string &end)
		{
			if ((begin.size() == 6) &&
					(end.size() == 6))
			{
				// HHMMSS
#ifdef DEBUG
				cout << "TimeValueRangeProcessor::operator: accepting " << begin << ".." << end << endl;
#endif
				return m_valueNumber;
			}
			if ((begin.size() == 8) && (end.size() == 8) &&
					(begin[2] == begin[5]) && (end[2] == end[5]) && (begin[2] == end[2]) &&
					(end[4] == ':'))
			{
				// HH:MM:SS
				begin.erase(2, 1);
				begin.erase(5, 1);
				end.erase(2, 1);
				end.erase(5, 1);
#ifdef DEBUG
				cout << "TimeValueRangeProcessor::operator: accepting " << begin << ".." << end << endl;
#endif
				return m_valueNumber;
			}
#ifdef DEBUG
			cout << "TimeValueRangeProcessor::operator: rejecting " << begin << ".." << end << endl;
#endif

			return Xapian::BAD_VALUENO;
		}

	protected:
		Xapian::valueno m_valueNumber;

};

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
}

XapianEngine::~XapianEngine()
{
}

Xapian::Query XapianEngine::parseQuery(Xapian::Database *pIndex, const QueryProperties &queryProps,
	const string &stemLanguage, DefaultOperator defaultOperator,
	string &correctedFreeQuery, bool minimal)
{
	Xapian::QueryParser parser;
	Xapian::Stem stemmer;
	unsigned int minDay, minMonth, minYear = 0;
	unsigned int maxDay, maxMonth, maxYear = 0;

	// Set things up
	if ((minimal == false) &&
		(stemLanguage.empty() == false))
	{
		try
		{
			stemmer = Xapian::Stem(StringManip::toLowerCase(stemLanguage));
		}
		catch (const Xapian::Error &error)
		{
			cerr << "Couldn't create stemmer: " << error.get_type() << ": " << error.get_msg() << endl;
		}
		parser.set_stemming_strategy(Xapian::QueryParser::STEM_ALL);
		parser.set_stemmer(stemmer);
	}
	else
	{
		parser.set_stemming_strategy(Xapian::QueryParser::STEM_NONE);
	}
	// What's the default operator ?
	if (defaultOperator == DEFAULT_OP_AND)
	{
		parser.set_default_op(Xapian::Query::OP_AND);
	}
	else
	{
		parser.set_default_op(Xapian::Query::OP_OR);
	}
	if (pIndex != NULL)
	{
		// The database is required for wildcards and spelling
		parser.set_database(*pIndex);
	}
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

	// What type of query is this ?
	QueryProperties::QueryType type = queryProps.getType();
	if (type != QueryProperties::XAPIAN_QP)
	{
		map<string, string> fieldMapping;

		// Bare minimum mapping between Xesam fields and our prefixes 
		fieldMapping["dc:title"] = "S";

		XapianQueryBuilder builder(parser, fieldMapping);
		XesamParser *pParser = NULL;

		// Get a Xesam parser
		if (type == QueryProperties::XESAM_QL)
		{
			pParser = new XesamQLParser();
		}
		else if (type == QueryProperties::XESAM_UL)
		{
			pParser = new XesamULParser();
		}

		if (pParser != NULL)
		{
			bool parsedQuery = pParser->parse(queryProps.getFreeQuery(), builder);

			delete pParser;

			if (parsedQuery == true)
			{
				return builder.get_query();
			}
		}

		return Xapian::Query();
	}

	// Do some pre-processing : look for filters with quoted values
	string freeQuery(StringManip::replaceSubString(queryProps.getFreeQuery(), "\n", " "));
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

	// Activate all necessary options
	unsigned int flags = Xapian::QueryParser::FLAG_BOOLEAN|Xapian::QueryParser::FLAG_PHRASE|
		Xapian::QueryParser::FLAG_LOVEHATE|Xapian::QueryParser::FLAG_BOOLEAN_ANY_CASE|
		Xapian::QueryParser::FLAG_PURE_NOT;
	if (minimal == false)
	{
		flags |= Xapian::QueryParser::FLAG_WILDCARD;
#if ENABLE_XAPIAN_SPELLING_CORRECTION>0
		flags |= Xapian::QueryParser::FLAG_SPELLING_CORRECTION;
#endif
	}

	// Date range
	Xapian::DateValueRangeProcessor dateProcessor(0);
	parser.add_valuerangeprocessor(&dateProcessor);

	// Size with a "b" suffix, ie 1024..10240b
#if XAPIAN_MAJOR_VERSION>1
	Xapian::NumberValueRangeProcessor sizeProcessor(2, "b", false);
	parser.add_valuerangeprocessor(&sizeProcessor);
#elif XAPIAN_MAJOR_VERSION==1
#if XAPIAN_MINOR_VERSION>0
	Xapian::NumberValueRangeProcessor sizeProcessor(2, "b", false);
#else
	// Xapian 1.02 is the bare minimum
	Xapian::v102::NumberValueRangeProcessor sizeProcessor(2, "b", false);
#endif
	parser.add_valuerangeprocessor(&sizeProcessor);
#endif

	// Time range
	TimeValueRangeProcessor timeProcessor(3);
	parser.add_valuerangeprocessor(&timeProcessor);

	// Parse the query string
	Xapian::Query parsedQuery = parser.parse_query(freeQuery, flags);
	if (minimal == true)
	{
		return parsedQuery;
	}

#if ENABLE_XAPIAN_SPELLING_CORRECTION>0
	// Any correction ?
	correctedFreeQuery = parser.get_corrected_query_string();
#ifdef DEBUG
	cout << "XapianEngine::parseQuery: corrected spelling to: " << correctedFreeQuery << endl;
#endif
#endif

	return parsedQuery;
}

bool XapianEngine::queryDatabase(Xapian::Database *pIndex, Xapian::Query &query,
	unsigned int startDoc, unsigned int maxResultsCount)
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
		Xapian::MSet matches = enquire.get_mset(startDoc, maxResultsCount, maxResultsCount + 1);
		if (matches.empty() == false)
		{
			m_resultsCountEstimate = matches.get_matches_estimated();
#ifdef DEBUG
			cout << "XapianEngine::queryDatabase: " << m_resultsCountEstimate << "/"
				<< maxResultsCount << " results found from position " << startDoc << endl;
#endif

			// Get the results
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
bool XapianEngine::runQuery(QueryProperties& queryProps,
	unsigned int startDoc)
{
	// Clear the results list
	m_resultsList.clear();
	m_resultsCountEstimate = 0;
	m_correctedFreeQuery.clear();

	if (queryProps.isEmpty() == true)
	{
#ifdef DEBUG
		cout << "XapianEngine::runQuery: query is empty" << endl;
#endif
		return false;
	}

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
		string stemLanguage(queryProps.getFilter("lang"));
		unsigned int searchStep = 1;

		// Searches are run in this order :
		// 1. don't stem terms
		// 2. if no results, stem terms if a language is defined for the query
		Xapian::Query fullQuery = parseQuery(pIndex, queryProps, "",
			m_defaultOperator, m_correctedFreeQuery);
		while (fullQuery.empty() == false)
		{
#ifdef DEBUG
			cout << "XapianEngine::runQuery: " << fullQuery.get_description() << endl;
#endif
			// Query the database
			if (queryDatabase(pIndex, fullQuery, startDoc,
				queryProps.getMaximumResultsCount()) == false)
			{
				break;
			}

			// The search did succeed but didn't return anything
			if ((m_resultsList.empty() == true) &&
				(searchStep == 1) &&
				(stemLanguage.empty() == false))
			{
#ifdef DEBUG
				cout << "XapianEngine::runQuery: trying again with stemming" << endl;
#endif
				fullQuery = parseQuery(pIndex, queryProps, Languages::toEnglish(stemLanguage),
					m_defaultOperator, m_correctedFreeQuery);
				++searchStep;
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
