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
#include "HtmlTokenizer.h"
#include "SherlockParser.h"
#include "StringManip.h"
#include "Url.h"
#include "FileCollector.h"
#include "PluginWebEngine.h"

// A function object to lower case map keys with for_each()
struct LowerAndCopy
{
	public:
		LowerAndCopy(map<string, string> &other) : m_other(other)
		{
		}

		void operator()(map<string, string>::value_type &p)
		{
			m_other[StringManip::toLowerCase(p.first)] = p.second;
		}

		map<string, string> &m_other;

};

PluginWebEngine::PluginWebEngine(const string &fileName) :
	WebEngine()
{
	m_skipLocal = true;
	m_nextFactor = 10;
	m_nextBase = 0;
	// SearchEngineInterface members
	m_maxResultsCount = 10;

	load(fileName);
}

PluginWebEngine::~PluginWebEngine()
{
}

bool PluginWebEngine::load(const string &fileName)
{
	if (fileName.empty() == true)
	{
		return false;
	}

	// Get the definition file
	FileCollector fileCollect;
	DocumentInfo docInfo("Plugin", string("file://") + fileName,
		"text/plain", "");
	Document *pPluginDoc = fileCollect.retrieveUrl(docInfo);
	if (pPluginDoc == NULL)
	{
#ifdef DEBUG
		cerr << "PluginWebEngine::load: couldn't load " << fileName << endl;
#endif
		return false;
	}

	SherlockParser parser(pPluginDoc);
	if (parser.parse() == false)
	{
		delete pPluginDoc;

		return false;
	}
	PluginProperties &properties = parser.getProperties();

	map<string, string> searchParams, interpretParams;

	// Lower case these maps' keys
	LowerAndCopy lowCopy1(searchParams);
	for_each(properties.m_searchParams.begin(), properties.m_searchParams.end(), lowCopy1);
	LowerAndCopy lowCopy2(interpretParams);
	for_each(properties.m_interpretParams.begin(), properties.m_interpretParams.end(), lowCopy2);

	map<string, string>::iterator mapIter = searchParams.find("name");
	if (mapIter != searchParams.end())
	{
		m_name = mapIter->second;
	}

	mapIter = searchParams.find("action");
	if (mapIter != searchParams.end())
	{
		m_baseUrl = mapIter->second;
	}

	mapIter = searchParams.find("routetype");
	if (mapIter != searchParams.end())
	{
		m_channel = mapIter->second;
	}

	copy(properties.m_inputItems.begin(), properties.m_inputItems.end(), inserter(m_inputTags, m_inputTags.begin()));
	if (properties.m_userInput.empty() == false)
	{
		// Remove the user input tag from the input tags map
		mapIter = m_inputTags.find(properties.m_userInput);
		if (mapIter != m_inputTags.end())
		{
			m_inputTags.erase(mapIter);
		}
		m_userInputTag = properties.m_userInput;
	}

	mapIter = interpretParams.find("resultliststart");
	if (mapIter != interpretParams.end())
	{
		m_resultListStart = StringManip::replaceSubString(mapIter->second, "\\n", "\n");
	}

	mapIter = interpretParams.find("resultlistend");
	if (mapIter != interpretParams.end())
	{
		m_resultListEnd = StringManip::replaceSubString(mapIter->second, "\\n", "\n");
	}

	mapIter = interpretParams.find("resultitemstart");
	if (mapIter != interpretParams.end())
	{
		m_resultItemStart = StringManip::replaceSubString(mapIter->second, "\\n", "\n");
	}

	mapIter = interpretParams.find("resultitemend");
	if (mapIter != interpretParams.end())
	{
		m_resultItemEnd = StringManip::replaceSubString(mapIter->second, "\\n", "\n");
	}

	mapIter = interpretParams.find("resulttitlestart");
	if (mapIter != interpretParams.end())
	{
		m_resultTitleStart = mapIter->second;
	}

	mapIter = interpretParams.find("resulttitleend");
	if (mapIter != interpretParams.end())
	{
		
		m_resultTitleEnd = mapIter->second;
	}

	mapIter = interpretParams.find("resultlinkstart");
	if (mapIter != interpretParams.end())
	{
		m_resultLinkStart = mapIter->second;
	}

	mapIter = interpretParams.find("resultlinkend");
	if (mapIter != interpretParams.end())
	{
		m_resultLinkEnd = mapIter->second;
	}

	mapIter = interpretParams.find("resultextractstart");
	if (mapIter != interpretParams.end())
	{
		m_resultExtractStart = mapIter->second;
	}

	mapIter = interpretParams.find("resultextractend");
	if (mapIter != interpretParams.end())
	{
		m_resultExtractEnd = mapIter->second;
	}

	mapIter = interpretParams.find("skiplocal");
	if (mapIter != interpretParams.end())
	{
		if (mapIter->second == "false")
		{
			m_skipLocal = false;
		}
	}

	m_nextTag = properties.m_nextInput;
	// Here we differ from how Mozilla uses these parameters
	// Normally, either factor or value is used, but we use value
	// as the parameter's initial value
	if (properties.m_nextFactor.empty() == false)
	{
		m_nextFactor = (unsigned int)atoi(properties.m_nextFactor.c_str());
	}
	if (properties.m_nextValue.empty() == false)
	{
		m_nextBase = (unsigned int)atoi(properties.m_nextValue.c_str());
	}

	delete pPluginDoc;

	return true;
}

bool PluginWebEngine::getPage(const string &formattedQuery)
{
	bool foundResult = false;

#ifdef DEBUG
	cout << "PluginWebEngine::getPage: getting " << formattedQuery << endl;
#endif
	DocumentInfo docInfo("Results Page", formattedQuery,
		"text/html", "");
	Document *pUrlDoc = downloadPage(docInfo);
	if (pUrlDoc == NULL)
	{
#ifdef DEBUG
		cerr << "PluginWebEngine::getPage: couldn't download page " << formattedQuery << endl;
#endif
		return false;
	}

	float pseudoScore = 100;
	unsigned int urlContentLen;
	const char *urlContent = pUrlDoc->getData(urlContentLen);
	if ((urlContent == NULL) ||
		(urlContentLen == 0))
	{
#ifdef DEBUG
		cerr << "PluginWebEngine::getPage: downloaded empty page" << endl;
#endif
		delete pUrlDoc;
		return false;
	}
#ifdef DEBUG
	ofstream pageBackup("PluginWebEngine.html");
	pageBackup.write(urlContent, urlContentLen);
	pageBackup.close();
#endif

	// Extract the results list
#ifdef DEBUG
	cout << "PluginWebEngine::getPage: getting results list (" << m_resultListStart << ", " << m_resultListEnd << ")" << endl;
#endif
	string resultList = StringManip::extractField(urlContent, m_resultListStart, m_resultListEnd);
	if (resultList.empty() == true)
	{
		resultList = string(urlContent, urlContentLen);
	}

	// Extract results
	string::size_type endPos = 0;
#ifdef DEBUG
	cout << "PluginWebEngine::getPage: getting first result (" << m_resultItemStart << ", " << m_resultItemEnd << ")" << endl;
#endif
	string resultItem = StringManip::extractField(resultList, m_resultItemStart, m_resultItemEnd, endPos);
	while ((resultItem.empty() == false) &&
		(m_resultsList.size() <= m_maxResultsCount))
	{
		string contentType, url, name, extract;

#ifdef DEBUG
		cout << "PluginWebEngine::getPage: candidate chunk \"" << resultItem << "\"" << endl;
#endif
		contentType = pUrlDoc->getType();
		if (strncasecmp(contentType.c_str(), "text/html", 9) == 0)
		{
			Document chunkDoc("", "", contentType, "");
			chunkDoc.setData(resultItem.c_str(), resultItem.length());
			HtmlTokenizer chunkTokens(&chunkDoc);
			set<Link> &chunkLinks = chunkTokens.getLinks();
			unsigned int endOfFirstLink = 0, startOfSecondLink = 0, endOfSecondLink = 0, startOfThirdLink = 0;

			// The result's URL and title should be given by the first link
			for (set<Link>::iterator linkIter = chunkLinks.begin(); linkIter != chunkLinks.end(); ++linkIter)
			{
				if (linkIter->m_pos == 0)
				{
					url = linkIter->m_url;
					name = linkIter->m_name;
#ifdef DEBUG
					cout << "PluginWebEngine::getPage: first link in chunk is " << url << endl;
#endif
					endOfFirstLink = linkIter->m_close;
				}
				else if (linkIter->m_pos == 1)
				{
					startOfSecondLink = linkIter->m_open;
					endOfSecondLink = linkIter->m_close;
				}
				else if (linkIter->m_pos == 2)
				{
					startOfThirdLink = linkIter->m_open;
				}
			}

			// Chances are the extract is between the first two links
			if (endOfFirstLink > 0)
			{
				string extractWithMarkup1, extractWithMarkup2;
				string extractCandidate1, extractCandidate2;

				if (startOfSecondLink > 0)
				{
					extractWithMarkup1 = resultItem.substr(endOfFirstLink, startOfSecondLink - endOfFirstLink);
				}
				else
				{
					extractWithMarkup1 = resultItem.substr(endOfFirstLink);
				}
				extractCandidate1 = HtmlTokenizer::stripTags(extractWithMarkup1);

				// ... or between the second and third link :-)
				if (endOfSecondLink > 0)
				{
					if (startOfThirdLink > 0)
					{
						extractWithMarkup2 = resultItem.substr(endOfSecondLink, startOfThirdLink - endOfSecondLink);
					}
					else
					{
						extractWithMarkup2 = resultItem.substr(endOfSecondLink);
					}
				}
				extractCandidate2 = HtmlTokenizer::stripTags(extractWithMarkup2);

				// It seems we can rely on length to determine which is the right one
				if (extractCandidate1.length() > extractCandidate2.length())
				{
					extract = extractCandidate1;
				}
				else
				{
					extract = extractCandidate2;
				}
#ifdef DEBUG
				cout << "PluginWebEngine::getPage: extract is \"" << extract << "\"" << endl;
#endif
			}
		}
		else
		{
			// This is not HTML
			// Use extended attributes
			if ((m_resultTitleStart.empty() == false) &&
				(m_resultTitleEnd.empty() == false))
			{
				name = StringManip::extractField(resultItem, m_resultTitleStart, m_resultTitleEnd);
			}

			if ((m_resultLinkStart.empty() == false) &&
				(m_resultLinkEnd.empty() == false))
			{
				url = StringManip::extractField(resultItem, m_resultLinkStart, m_resultLinkEnd);
			}

			if ((m_resultExtractStart.empty() == false) &&
				(m_resultExtractEnd.empty() == false))
			{
				extract = StringManip::extractField(resultItem, m_resultExtractStart, m_resultExtractEnd);
			}
		}

		if (url.empty() == false)
		{
			Url urlObj(url);

			// Is this URL relative to the search engine's domain ?
			// FIXME: look for a interpret/baseurl tag, see https://bugzilla.mozilla.org/show_bug.cgi?id=65453
			// FIXME: obey m_skipLocal
			if (urlObj.getHost().empty() == true)
			{
				Url baseUrlObj(formattedQuery);

				string tmpUrl = baseUrlObj.getProtocol();
				tmpUrl += "://";
				tmpUrl += baseUrlObj.getHost();
				if (url[0] != '/')
				{
					tmpUrl += "/";
				}
				tmpUrl += url;
				url = tmpUrl;
			}

			if (processResult(url) == true)
			{
				m_resultsList.push_back(Result(url, name, extract, "", pseudoScore));
			}
			--pseudoScore;
			foundResult = true;
		}

		// Next
		endPos += m_resultItemEnd.length();
		resultItem = StringManip::extractField(resultList, m_resultItemStart, m_resultItemEnd, endPos);
	}
	delete pUrlDoc;

	return foundResult;
}

bool PluginWebEngine::getDetails(const string &fileName, string &name, string &channel)
{
	if (fileName.empty() == true)
	{
		return false;
	}

	// Get the definition file
	FileCollector fileCollect;
	DocumentInfo docInfo(name, string("file://") + fileName,
		"text/plain", "");
	Document *pPluginDoc = fileCollect.retrieveUrl(docInfo);
	if (pPluginDoc == NULL)
	{
#ifdef DEBUG
		cerr << "PluginWebEngine::getDetails: couldn't load " << fileName << endl;
#endif
		return false;
	}

	SherlockParser parser(pPluginDoc);
	if (parser.parse(true) == false)
	{
#ifdef DEBUG
		cerr << "PluginWebEngine::getDetails: couldn't parse " << fileName << endl;
#endif
		delete pPluginDoc;

		return false;
	}
	PluginProperties &properties = parser.getProperties();

	map<string, string> searchParams;

	LowerAndCopy lowCopy1(searchParams);
	for_each(properties.m_searchParams.begin(), properties.m_searchParams.end(), lowCopy1);

	map<string, string>::iterator mapIter = searchParams.find("name");
	if (mapIter != searchParams.end())
	{
		name = mapIter->second;
	}
	mapIter = searchParams.find("routetype");
	if (mapIter != searchParams.end())
	{
		channel = mapIter->second;
	}

	delete pPluginDoc;

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

	if (m_userInputTag.empty() == true)
	{
#ifdef DEBUG
		cout << "PluginWebEngine::runQuery: no user input tag" << endl;
#endif
		return false;
	}
	if (queryString.empty() == true)
	{
#ifdef DEBUG
		cout << "PluginWebEngine::runQuery: query is empty" << endl;
#endif
		return false;
	}

	string formattedQuery = m_baseUrl;
	formattedQuery += "?";
	formattedQuery += m_userInputTag;
	formattedQuery += "=";
	formattedQuery += queryString;
	for (map<string, string>::iterator tagIter = m_inputTags.begin(); tagIter != m_inputTags.end(); ++tagIter)
	{
		formattedQuery += "&";
		formattedQuery += tagIter->first;
		formattedQuery += "=";
		formattedQuery += tagIter->second;
	}

	setHostNameFilter(queryProps.getHostFilter());
	setFileNameFilter(queryProps.getFileFilter());

#ifdef DEBUG
	cout << "PluginWebEngine::runQuery: querying " << m_name << endl;
#endif
	while (count < m_maxResultsCount)
	{
		string pageQuery = formattedQuery;

		if (m_nextTag.empty() == false)
		{
			char factorStr[64];

			// Is the INPUTNEXT FACTOR set to zero ?
			if (m_nextFactor == 0)
			{
				// Assume INPUTNEXT allows to specify a number of results
				// Not sure if this is how Sherlock/Mozilla interpret this
				pageQuery += "&";
				pageQuery += m_nextTag;
				pageQuery += "=";
				snprintf(factorStr, 64, "%u", m_maxResultsCount);
				pageQuery += factorStr;
			}
			else
			{
				pageQuery += "&";
				pageQuery += m_nextTag;
				pageQuery += "=";
				snprintf(factorStr, 64, "%u", currentFactor + m_nextBase);
				pageQuery += factorStr;
			}
		}

		if (getPage(pageQuery) == false)
		{
			break;
		}

		if (m_nextFactor == 0)
		{
			// That one page should have all the results...
#ifdef DEBUG
			cout << "PluginWebEngine::runQuery: performed one off call" << endl;
#endif
			break;
		}
		else
		{
			if (m_resultsList.size() < count + m_nextFactor)
			{
				// We got less than the maximum number of results per page
				// so there's no point in requesting the next page
#ifdef DEBUG
				cout << "PluginWebEngine::runQuery: last page wasn't full" << endl;
#endif
				break;
			}

			// Increase factor
			currentFactor += m_nextFactor;
		}
		count = m_resultsList.size();
	}

	return true;
}
