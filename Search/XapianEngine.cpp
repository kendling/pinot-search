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

#include "StringManip.h"
#include "Tokenizer.h"
#include "Url.h"
#include "XapianDatabaseFactory.h"
#include "XapianEngine.h"

using std::string;
using std::vector;
using std::stack;
using std::cout;
using std::cerr;
using std::endl;

static bool extractWords(const string &text, const string &language, vector<string> &wordsList)
{
	Xapian::Stem *pStemmer = NULL;

	if (text.empty() == true)
	{
		return false;
	}

	if (language.empty() == false)
	{
		pStemmer = new Xapian::Stem(StringManip::toLowerCase(language));
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

bool XapianEngine::queryDatabase(Xapian::Query &query)
{
	bool bStatus = false;

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, true);
	if (pDatabase == NULL)
	{
		return false;
	}

	Xapian::Database *pIndex = pDatabase->readLock();
	if (pIndex != NULL)
	{
		try
		{
			// Start an enquire session on the database
			Xapian::Enquire enquire(*pIndex);

			// Give the query object to the enquire session
			enquire.set_query(query);

			// Get the top N results of the query
			Xapian::MSet matches = enquire.get_mset(0, m_maxResultsCount);

			// Get the results
#ifdef DEBUG
			cout << "XapianEngine::queryDatabase: " << matches.get_matches_estimated() << "/" << m_maxResultsCount << " results found" << endl;
#endif
			for (Xapian::MSetIterator i = matches.begin(); i != matches.end(); ++i)
			{
				// Get the document data
				string record = i.get_document().get_data();

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
					url = buildUrl(m_databaseName, *i);
				}
				else
				{
#ifdef DEBUG
					cout << "XapianEngine::queryDatabase: found omindex URL " << url << endl;
#endif
					url = Url::canonicalizeUrl(url);
				}

				// Get the summary and the type
				string summary = StringManip::extractField(record, "sample=", "\n");
#ifdef DEBUG
				cout << "XapianEngine::queryDatabase: found omindex summary " << summary << endl;
#endif
				string type = StringManip::extractField(record, "type=", "\n");
				// ...and finally the language, if available
				string language = StringManip::extractField(record, "language=", "\n");

				// Add this result
				Result thisResult(url, title, summary, language, (float)i.get_percent());
				m_resultsList.push_back(thisResult);
			}

			bStatus = true;
		}
		catch (const Xapian::Error &error)
		{
			cout << "XapianEngine::queryDatabase: couldn't run query: "  << error.get_msg() << endl;
		}
	}
	pDatabase->unlock();

	return bStatus;
}

void XapianEngine::stackQuery(const QueryProperties &queryProps,
	stack<Xapian::Query> &queryStack, bool followOperators)
{
	string language = queryProps.getLanguage();
	Xapian::Query::op queryOp = Xapian::Query::OP_OR;
	string term;

	// Get the terms to AND together
	if (queryProps.getAndWords().empty() == false)
	{
		vector<string> andTerms;

		if (extractWords(queryProps.getAndWords(), language, andTerms) == true)
		{
#ifdef DEBUG
			cout << "XapianEngine::stackQuery: OP_AND "  << andTerms.size() << endl;
#endif
			if (followOperators == true)
			{
				queryOp = Xapian::Query::OP_AND;
			}
			queryStack.push(Xapian::Query(queryOp, andTerms.begin(), andTerms.end()));
		}
	}

	// Get the terms of the phrase
	if (queryProps.getPhrase().empty() == false)
	{
		vector<string> phraseTerms;

		if (extractWords(queryProps.getPhrase(), language, phraseTerms) == true)
		{
#ifdef DEBUG
			cout << "XapianEngine::stackQuery: OP_PHRASE "  << phraseTerms.size() << endl;
#endif
			if (followOperators == true)
			{
				queryOp = Xapian::Query::OP_PHRASE;
			}
			queryStack.push(Xapian::Query(queryOp, phraseTerms.begin(), phraseTerms.end()));
		}
	}

	// Get the terms to OR together
	if (queryProps.getAnyWords().empty() == false)
	{
		vector<string> orTerms;

		if (extractWords(queryProps.getAnyWords(), language, orTerms) == true)
		{
#ifdef DEBUG
			cout << "XapianEngine::stackQuery: OP_OR "  << orTerms.size() << endl;
#endif
			if (followOperators == true)
			{
				queryOp = Xapian::Query::OP_OR;
			}
			queryStack.push(Xapian::Query(queryOp, orTerms.begin(), orTerms.end()));
		}
	}

	// Get the terms to NOT together
	if (queryProps.getNotWords().empty() == false)
	{
		vector<string> notTerms;

		if (extractWords(queryProps.getNotWords(), language, notTerms) == true)
		{
#ifdef DEBUG
			cout << "XapianEngine::stackQuery: OP_AND_NOT "  << notTerms.size() << endl;
#endif
			// An AND_NOT has to have two sub-queries
			if (followOperators == true)
			{
				queryOp = Xapian::Query::OP_AND;
			}
			Xapian::Query notQuery(queryOp, notTerms.begin(), notTerms.end());
			// We need something to AND_NOT these terms against
			if (queryStack.empty() == false)
			{
				Xapian::Query topQuery = queryStack.top();
				queryStack.pop();
				if (followOperators == true)
				{
					queryOp = Xapian::Query::OP_AND_NOT;
				}
				queryStack.push(Xapian::Query(queryOp, topQuery, notQuery));
			}
		}
	}

	// Get the host name filter
	if (queryProps.getHostFilter().empty() == false)
	{
		vector<string> hostTerms;

		term = "H";
		term += StringManip::toLowerCase(queryProps.getHostFilter());
		hostTerms.push_back(term);
		if (followOperators == true)
		{
			queryOp = Xapian::Query::OP_AND;
		}
		queryStack.push(Xapian::Query(queryOp, hostTerms.begin(), hostTerms.end()));
	}

	// Get the file name filter
	if (queryProps.getFileFilter().empty() == false)
	{
		vector<string> fileTerms;

		term = "F";
		term += StringManip::toLowerCase(queryProps.getFileFilter());
		fileTerms.push_back(term);
		if (followOperators == true)
		{
			queryOp = Xapian::Query::OP_AND;
		}
		queryStack.push(Xapian::Query(queryOp, fileTerms.begin(), fileTerms.end()));
	}

	// Get the label name filter
	if (queryProps.getLabelFilter().empty() == false)
	{
		vector<string> labelTerms;

		term = "C";
		term += queryProps.getLabelFilter();
		labelTerms.push_back(term);
		if (followOperators == true)
		{
			queryOp = Xapian::Query::OP_AND;
		}
		queryStack.push(Xapian::Query(queryOp, labelTerms.begin(), labelTerms.end()));
	}

	// Get the language filter
	if (queryProps.getLanguage().empty() == false)
	{
		vector<string> languageTerms;

		term = "L";
		term += StringManip::toLowerCase(queryProps.getLanguage());
		languageTerms.push_back(term);
		if (followOperators == true)
		{
			queryOp = Xapian::Query::OP_AND;
		}
		queryStack.push(Xapian::Query(queryOp, languageTerms.begin(), languageTerms.end()));
	}
}

/// Returns the URL for the given document in the given index.
string XapianEngine::buildUrl(const string &database, unsigned int docId)
{
	// Make up a pseudo URL
	char docIdStr[64];
	sprintf(docIdStr, "%u", docId);
	string url = "xapian://localhost/";
	url += database;
	url += "/";
	url += docIdStr;

	return url;
}

/// Runs a boolean query; true if success.
bool XapianEngine::runQuery(const string &keyword)
{
	// Clear the results list
	m_resultsList.clear();

	try
	{
		vector<string> keywordTerms;
		keywordTerms.push_back(keyword);
		Xapian::Query keyworkQuery(Xapian::Query::OP_AND, keywordTerms.begin(), keywordTerms.end());

		// Query the database with the full query
		return queryDatabase(keyworkQuery);
	}
	catch (const Xapian::Error &error)
	{
		cout << "XapianEngine::runQuery: couldn't run query: "  << error.get_msg() << endl;
	}

	return false;
}

//
// Implementation of SearchEngineInterface
//

/// Runs a query; true if success.
bool XapianEngine::runQuery(QueryProperties& queryProps)
{
	// Clear the results list
	m_resultsList.clear();

	try
	{
		stack<Xapian::Query> queryStack;
		bool followOperators = true;

		stackQuery(queryProps, queryStack, followOperators);
		while (queryStack.empty() == false)
		{
			while (queryStack.size() > 1)
			{
				Xapian::Query topQuery = queryStack.top();
				queryStack.pop();
#ifdef DEBUG
				cout << "XapianEngine::runQuery: popped query, left "  << queryStack.size() << endl;
#endif
				Xapian::Query query = Xapian::Query(Xapian::Query::OP_AND, queryStack.top(), topQuery);
				queryStack.pop();
#ifdef DEBUG
				cout << "XapianEngine::runQuery: popped query, left "  << queryStack.size() << endl;
#endif
				queryStack.push(query);
			}

			// Query the database with the full query
			if (queryDatabase(queryStack.top()) == true)
			{
#if 0
				if ((m_resultsList.empty() == true) &&
					(followOperators == true))
				{
					// The search did succeed but didn't return anything
					// Try again by OR'ing terms together
					while (queryStack.empty() == false)
					{
						queryStack.pop();
					}
					followOperators = false;
#ifdef DEBUG
					cout << "XapianEngine::runQuery: trying with OR'ed terms" << endl;
#endif
					stackQuery(queryProps, queryStack, followOperators);
					continue;
				}
#endif

				return true;
			}
		}
	}
	catch (const Xapian::Error &error)
	{
		cout << "XapianEngine::runQuery: couldn't run query: "  << error.get_msg() << endl;
	}

	return false;
}

/// Returns the results for the previous query.
const vector<Result> &XapianEngine::getResults(void) const
{
	return m_resultsList;
}
