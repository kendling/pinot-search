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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>

#include "Languages.h"
#include "StringManip.h"
#include "Tokenizer.h"
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

static bool extractWords(const string &text, const string &stemLanguage, vector<string> &wordsList)
{
	Xapian::Stem *pStemmer = NULL;

	if (text.empty() == true)
	{
		return false;
	}

	if (stemLanguage.empty() == false)
	{
		pStemmer = new Xapian::Stem(StringManip::toLowerCase(stemLanguage));
	}

	Document doc;
	doc.setData(text.c_str(), text.length());
	Tokenizer tokens(&doc);

	string token;
	while (tokens.nextToken(token) == true)
	{
		string term = token;

		// Lower case the term
		term = StringManip::toLowerCase(term);
		// Stem it ?
		if (pStemmer != NULL)
		{
			string stemmedTerm = pStemmer->stem_word(term);
			wordsList.push_back(stemmedTerm);
		}
		else
		{
			wordsList.push_back(term);
		}
	}

	if (pStemmer != NULL)
	{
		delete pStemmer;
	}

	return true;
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

Xapian::Query XapianEngine::parseQuery(Xapian::Database *pIndex, const QueryProperties &queryProps,
	const string &stemLanguage, bool followOperators)
{
	string freeQuery(StringManip::replaceSubString(queryProps.getFreeQuery(), "\n", " "));
	Xapian::QueryParser parser;
	Xapian::Stem stemmer;

	if (stemLanguage.empty() == false)
	{
		stemmer = Xapian::Stem(StringManip::toLowerCase(stemLanguage));
	}

	// Set things up
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
	parser.add_boolean_prefix("site", "H");
	parser.add_boolean_prefix("file", "P");
	parser.add_boolean_prefix("title", "S");
	parser.add_boolean_prefix("url", "U");
	parser.add_boolean_prefix("dir", "XDIR");
	parser.add_boolean_prefix("lang", "L");
	parser.add_boolean_prefix("type", "T");
	parser.add_boolean_prefix("label", "XLABEL");

	// Activate all options and parse
	return parser.parse_query(freeQuery,
		Xapian::QueryParser::FLAG_BOOLEAN|Xapian::QueryParser::FLAG_PHRASE|Xapian::QueryParser::FLAG_LOVEHATE|Xapian::QueryParser::FLAG_BOOLEAN_ANY_CASE|Xapian::QueryParser::FLAG_WILDCARD);
}

/// Validates a query and extracts its terms.
bool XapianEngine::validateQuery(QueryProperties& queryProps, bool includePrefixed,
	set<string> &terms)
{
	bool goodQuery = false;

	try
	{
		Xapian::Query freeQuery = parseQuery(NULL, queryProps, "", true);
		if (freeQuery.empty() == false)
		{
			for (Xapian::TermIterator termIter = freeQuery.get_terms_begin();
				termIter != freeQuery.get_terms_end(); ++termIter)
			{
				// Skip prefixed terms unless instructed otherwise
				if ((includePrefixed == true) ||
					(isupper((int)((*termIter)[0])) == 0))
				{
					terms.insert(*termIter);
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
					Xapian::weight termWeight = maxWeight - matches.get_termweight(termName);

					queryTerms.insert(pair<Xapian::weight, string>(termWeight, termName));
#ifdef DEBUG
					cout << "XapianEngine::queryDatabase: term " << termName
						<< " has weight " << matches.get_termweight(termName) << "/" << maxWeight << endl;
#endif
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

					// Finally, get a summary
					string summary = StringManip::extractField(record, "sample=", "\n");
					if (summary.empty() == true)
					{
						AbstractGenerator abstractGen(pIndex, 50);

						// Generate an abstract based on the query's terms
						summary = abstractGen.generateAbstract(seedTerms, docId);
					}

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
		Xapian::Query freeQuery = parseQuery(pIndex, queryProps, "", followOperators);
		while (freeQuery.empty() == false)
		{
#ifdef DEBUG
			cout << "XapianEngine::runQuery: query terms are " << endl;
			for (Xapian::TermIterator termIter = freeQuery.get_terms_begin();
				termIter != freeQuery.get_terms_end(); ++termIter)
			{
				cout << " " << *termIter << endl;
			}
#endif
			// Query the database
			if (queryDatabase(pIndex, freeQuery) == false)
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
				freeQuery = parseQuery(pIndex, queryProps,
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

