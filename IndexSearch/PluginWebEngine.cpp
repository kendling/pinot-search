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

#include <algorithm>
#include <fstream>
#include <iostream>
#include <cstring>

#include "config.h"
#include "Document.h"
#include "StringManip.h"
#include "Url.h"
#include "OpenSearchParser.h"
#ifdef HAVE_BOOST_SPIRIT
#include "SherlockParser.h"
#endif
#include "PluginWebEngine.h"

using std::cout;
using std::cerr;
using std::endl;

PluginWebEngine::PluginWebEngine(const string &fileName) :
	WebEngine(),
	m_pResponseParser(NULL)
{
	load(fileName);
}

PluginWebEngine::~PluginWebEngine()
{
	if (m_pResponseParser != NULL)
	{
		delete m_pResponseParser;
	}
}

void PluginWebEngine::load(const string &fileName)
{
	if (fileName.empty() == true)
	{
		return;
	}

	PluginParserInterface *pParser = getPluginParser(fileName);
	if (pParser == NULL)
	{
		return;
	}

	m_pResponseParser = pParser->parse(m_properties);
	delete pParser;
}

bool PluginWebEngine::getPage(const string &formattedQuery, unsigned int maxResultsCount)
{
	if ((m_pResponseParser == NULL) ||
		(formattedQuery.empty() == true))
	{
		return false;
	}

	DocumentInfo docInfo("Results Page", formattedQuery,
		"text/html", "");
	Document *pResponseDoc = downloadPage(docInfo);
	if (pResponseDoc == NULL)
	{
		cerr << "PluginWebEngine::getPage: couldn't download "
			<< formattedQuery << endl;
		return false;
	}

	unsigned int contentLen;
	const char *pContent = pResponseDoc->getData(contentLen);
	if ((pContent == NULL) ||
		(contentLen == 0))
	{
#ifdef DEBUG
		cout << "PluginWebEngine::getPage: downloaded empty page" << endl;
#endif
		delete pResponseDoc;
		return false;
	}
#ifdef DEBUG
	Url urlObj(formattedQuery);
	string fileName(urlObj.getHost() + "_PluginWebEngine.html");

	ofstream pageBackup(fileName.c_str());
	pageBackup.write(pContent, contentLen);
	pageBackup.close();
#endif

	bool success = m_pResponseParser->parse(pResponseDoc, m_resultsList,
		maxResultsCount, m_properties.m_nextBase);
	vector<DocumentInfo>::iterator resultIter = m_resultsList.begin();
	while (resultIter != m_resultsList.end())
	{
		if (processResult(formattedQuery, *resultIter) == false)
		{
			// Remove this result
			if (resultIter == m_resultsList.begin())
			{
				m_resultsList.erase(resultIter);
				resultIter = m_resultsList.begin();
			}
			else
			{
				vector<DocumentInfo>::iterator badResultIter = resultIter;
				--resultIter;
				m_resultsList.erase(badResultIter);
			}
		}
		else
		{
			// Next
			++resultIter;
		}
	}

	delete pResponseDoc;

	return success;
}

PluginParserInterface *PluginWebEngine::getPluginParser(const string &fileName)
{
	if (fileName.empty() == true)
	{
		return NULL;
	}

	// What type of plugin is it ?
	// Look at the file extension
	string::size_type pos = fileName.find_last_of(".");
	if (pos == string::npos)
	{
		// No way to tell
		return NULL;
	}

	string extension(fileName.substr(pos + 1));
#ifdef HAVE_BOOST_SPIRIT_CORE_HPP
	if (strncasecmp(extension.c_str(), "src", 3) == 0)
	{
		return new SherlockParser(fileName);
	}
	else
#endif
	if (strncasecmp(extension.c_str(), "xml", 3) == 0)
	{
		return new OpenSearchParser(fileName);
	}

	return NULL;
}

bool PluginWebEngine::getDetails(const string &fileName, string &name, string &channel)
{
	if (fileName.empty() == true)
	{
		return false;
	}

	PluginParserInterface *pParser = getPluginParser(fileName);
	if (pParser == NULL)
	{
		return false;
	}

	SearchPluginProperties properties;
	ResponseParserInterface *pResponseParser = pParser->parse(properties, true);
	if (pResponseParser == NULL)
	{
		cerr << "PluginWebEngine::getDetails: couldn't parse "
			<< fileName << endl;
		delete pParser;

		return false;
	}
	delete pResponseParser;
	delete pParser;

	if (properties.m_response == SearchPluginProperties::UNKNOWN_RESPONSE)
	{
#ifdef DEBUG
		cout << "PluginWebEngine::getDetails: bad response type for "
			<< fileName << endl;
#endif
		return false;
	}

	name = properties.m_name;
	channel = properties.m_channel;

	return true;
}

//
// Implementation of SearchEngineInterface
//

/// Runs a query; true if success.
bool PluginWebEngine::runQuery(QueryProperties& queryProps,
	unsigned int startDoc)
{
	string queryString(queryProps.getFreeQuery(true));
	char countStr[64];
	unsigned int maxResultsCount(queryProps.getMaximumResultsCount());
	unsigned int currentIncrement = 0, count = 0;

	m_resultsList.clear();
	m_resultsCountEstimate = 0;

	if (queryProps.getType() != QueryProperties::XAPIAN_QP)
	{
		cerr << "PluginWebEngine::runQuery: query type not supported" << endl;
		return false;
	}

	if (queryString.empty() == true)
	{
#ifdef DEBUG
		cout << "PluginWebEngine::runQuery: query is empty" << endl;
#endif
		return false;
	}

	map<SearchPluginProperties::Parameter, string>::iterator paramIter = m_properties.m_parameters.find(SearchPluginProperties::SEARCH_TERMS_PARAM);
	if (paramIter == m_properties.m_parameters.end())
	{
#ifdef DEBUG
		cout << "PluginWebEngine::runQuery: no user input tag" << endl;
#endif
		return false;
	}

	string userInputTag(paramIter->second);
	string formattedQuery = m_properties.m_baseUrl;
	formattedQuery += "?";
	formattedQuery += userInputTag;
	formattedQuery += "=";
	formattedQuery += queryString;
	if (m_properties.m_parametersRemainder.empty() == false)
	{
		formattedQuery += "&";
		formattedQuery += m_properties.m_parametersRemainder;
	}

	setQuery(queryProps);

#ifdef DEBUG
	cout << "PluginWebEngine::runQuery: querying "
		<< m_properties.m_name << endl;
#endif
	while (count < maxResultsCount)
	{
		string pageQuery(formattedQuery);

		// How do we scroll ?
		if (m_properties.m_scrolling == SearchPluginProperties::PER_INDEX)
		{
			paramIter = m_properties.m_parameters.find(SearchPluginProperties::COUNT_PARAM);
			if ((paramIter != m_properties.m_parameters.end()) &&
				(paramIter->second.empty() == false))
			{
				// Number of results requested
				pageQuery += "&";
				pageQuery += paramIter->second;
				pageQuery += "=";
				snprintf(countStr, 64, "%u", maxResultsCount);
				pageQuery += countStr;
			}

			paramIter = m_properties.m_parameters.find(SearchPluginProperties::START_INDEX_PARAM);
			if ((paramIter != m_properties.m_parameters.end()) &&
				(paramIter->second.empty() == false))
			{
				// The offset of the first result (typically 1 or 0)
				pageQuery += "&";
				pageQuery += paramIter->second;
				pageQuery += "=";
				snprintf(countStr, 64, "%u", count + m_properties.m_nextBase);
				pageQuery += countStr;
			}
		}
		else
		{
			paramIter = m_properties.m_parameters.find(SearchPluginProperties::START_PAGE_PARAM);
			if ((paramIter != m_properties.m_parameters.end()) &&
				(paramIter->second.empty() == false))
			{
				// The offset of the page
				pageQuery += "&";
				pageQuery += paramIter->second;
				pageQuery += "=";
				snprintf(countStr, 64, "%u", currentIncrement + m_properties.m_nextBase);
				pageQuery += countStr;
			}
		}

		if (getPage(pageQuery, queryProps.getMaximumResultsCount()) == false)
		{
			break;
		}

		if (m_properties.m_nextIncrement == 0)
		{
			// That one page should have all the results...
#ifdef DEBUG
			cout << "PluginWebEngine::runQuery: performed one off call" << endl;
#endif
			break;
		}
		else
		{
			if (m_resultsList.size() < count + m_properties.m_nextIncrement)
			{
				// We got less than the maximum number of results per page
				// so there's no point in requesting the next page
#ifdef DEBUG
				cout << "PluginWebEngine::runQuery: last page wasn't full" << endl;
#endif
				break;
			}

			// Increase factor
			currentIncrement += m_properties.m_nextIncrement;
		}
		count = m_resultsList.size();
	}

	return true;
}
