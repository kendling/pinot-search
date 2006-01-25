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

#include <algorithm>
#include <fstream>
#include <iostream>

#include "Document.h"
#include "StringManip.h"
#include "OpenSearchParser.h"
#include "SherlockParser.h"
#include "PluginWebEngine.h"

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

bool PluginWebEngine::getPage(const string &formattedQuery)
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
#ifdef DEBUG
		cerr << "PluginWebEngine::getPage: couldn't download page "
			<< formattedQuery << endl;
#endif
		return false;
	}

	unsigned int contentLen;
	const char *pContent = pResponseDoc->getData(contentLen);
	if ((pContent == NULL) ||
		(contentLen == 0))
	{
#ifdef DEBUG
		cerr << "PluginWebEngine::getPage: downloaded empty page" << endl;
#endif
		delete pResponseDoc;
		return false;
	}
#ifdef DEBUG
	ofstream pageBackup("PluginWebEngine.html");
	pageBackup.write(pContent, contentLen);
	pageBackup.close();
#endif

	bool success = m_pResponseParser->parse(pResponseDoc, m_resultsList, m_maxResultsCount);
	vector<Result>::iterator resultIter = m_resultsList.begin();
	while (resultIter != m_resultsList.end())
	{
		string url(resultIter->getLocation());

		if (processResult(url) == false)
		{
			// Remove this result
			if (resultIter == m_resultsList.begin())
			{
				m_resultsList.erase(resultIter);
				resultIter = m_resultsList.begin();
			}
			else
			{
				vector<Result>::iterator badResultIter = resultIter;
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
	if (strncasecmp(extension.c_str(), "src", 3) == 0)
	{
		return new SherlockParser(fileName);
	}
	else if (strncasecmp(extension.c_str(), "xml", 3) == 0)
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
	if (pParser->parse(properties, true) == false)
	{
#ifdef DEBUG
		cerr << "PluginWebEngine::getDetails: couldn't parse " << fileName << endl;
#endif
		delete pParser;

		return false;
	}
	delete pParser;

	name = properties.m_name;
	channel = properties.m_channel;

	return true;
}

//
// Implementation of SearchEngineInterface
//

/// Runs a query; true if success.
bool PluginWebEngine::runQuery(QueryProperties& queryProps)
{
	string queryString = queryProps.toString(false);
	unsigned int currentFactor = 0, count = 0;

	m_resultsList.clear();

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
	formattedQuery += "&";
	formattedQuery += m_properties.m_parametersRemainder;

	setHostNameFilter(queryProps.getHostFilter());
	setFileNameFilter(queryProps.getFileFilter());

#ifdef DEBUG
	cout << "PluginWebEngine::runQuery: querying "
		<< m_properties.m_name << endl;
#endif
	while (count < m_maxResultsCount)
	{
		string pageQuery = formattedQuery;

		if (m_properties.m_nextTag.empty() == false)
		{
			char factorStr[64];

			// Is the INPUTNEXT FACTOR set to zero ?
			if (m_properties.m_nextFactor == 0)
			{
				// Assume INPUTNEXT allows to specify a number of results
				// Not sure if this is how Sherlock/Mozilla interpret this
				pageQuery += "&";
				pageQuery += m_properties.m_nextTag;
				pageQuery += "=";
				snprintf(factorStr, 64, "%u", m_maxResultsCount);
				pageQuery += factorStr;
			}
			else
			{
				pageQuery += "&";
				pageQuery += m_properties.m_nextTag;
				pageQuery += "=";
				snprintf(factorStr, 64, "%u", currentFactor + m_properties.m_nextBase);
				pageQuery += factorStr;
			}
		}

		if (getPage(pageQuery) == false)
		{
			break;
		}

		if (m_properties.m_nextFactor == 0)
		{
			// That one page should have all the results...
#ifdef DEBUG
			cout << "PluginWebEngine::runQuery: performed one off call" << endl;
#endif
			break;
		}
		else
		{
			if (m_resultsList.size() < count + m_properties.m_nextFactor)
			{
				// We got less than the maximum number of results per page
				// so there's no point in requesting the next page
#ifdef DEBUG
				cout << "PluginWebEngine::runQuery: last page wasn't full" << endl;
#endif
				break;
			}

			// Increase factor
			currentFactor += m_properties.m_nextFactor;
		}
		count = m_resultsList.size();
	}

	return true;
}
