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

#include "Url.h"
#include "SearchEngineInterface.h"

using std::cout;
using std::endl;

SearchEngineInterface::SearchEngineInterface() :
	m_callsCount(0),
	m_startTime(time(NULL)),
	m_maxResultsCount(10)
{
}

SearchEngineInterface::~SearchEngineInterface()
{
}

/// Indicates if the search engine is available in several languages/countries.
bool SearchEngineInterface::isInternational(void) const
{
	return false;
}

/// Sets the search engine's key, if applicable.
void SearchEngineInterface::setKey(const string &key)
{
	m_key = key;
}

/// Sets the number of calls issued since start time.
void SearchEngineInterface::setCallsCount(unsigned int count, time_t startTime)
{
	m_callsCount = count;
	m_startTime = startTime;
}

/// Sets the maximum number of results to return.
void SearchEngineInterface::setMaxResultsCount(unsigned int count)
{
	m_maxResultsCount = count;
}

/// Returns the results for the previous query.
const vector<Result> &SearchEngineInterface::getResults(void) const
{
	return m_resultsList;
}

/// Returns the charset for the previous query's results.
string SearchEngineInterface::getResultsCharset(void) const
{
	return m_charset;
}

void SearchEngineInterface::setHostNameFilter(const string &filter)
{
	m_hostFilter = filter;
}

void SearchEngineInterface::setFileNameFilter(const string &filter)
{
	m_fileFilter = filter;
}

bool SearchEngineInterface::processResult(const string &queryUrl, string &resultUrl)
{
	Url queryUrlObj(queryUrl);
	string queryHost(Url::reduceHost(queryUrlObj.getHost(), 2));

	if (resultUrl.empty() == true)
	{
		return false;
	}

	if ((resultUrl[0] == '/') ||
		((resultUrl.length() > 1) &&
		(resultUrl[0] == '.') &&
		(resultUrl[1] == '/')))
	{
		string fullResultUrl(queryUrlObj.getProtocol());

		fullResultUrl += "://";
		fullResultUrl += queryUrlObj.getHost();
		if (resultUrl[0] == '.')
		{
			fullResultUrl += resultUrl.substr(1);
		}
		else
		{
			fullResultUrl += resultUrl;
		}

		resultUrl = fullResultUrl;
	}

	Url resultUrlObj(resultUrl);

	if ((m_hostFilter.empty() == false) &&
		(resultUrlObj.getHost() != m_hostFilter))
	{
#ifdef DEBUG
		cout << "SearchEngineInterface::processResult: skipping " << resultUrl << endl;
#endif
		return false;
	}

	if ((m_fileFilter.empty() == false) &&
		(resultUrlObj.getFile() != m_fileFilter))
	{
#ifdef DEBUG
		cout << "SearchEngineInterface::processResult: skipping " << resultUrl << endl;
#endif
		return false;
	}

	// Is the result's host name the same as the search engine's ?
	// FIXME: not all TLDs have leafs at level 2
	if (queryHost == Url::reduceHost(resultUrlObj.getHost(), 2))
	{
		string protocol(resultUrlObj.getProtocol());

		if (protocol.empty() == false)
		{
			string embeddedUrl;

			string::size_type startPos = resultUrl.find(protocol, protocol.length());
			if (startPos != string::npos)
			{
				string::size_type endPos = resultUrl.find("&amp;", startPos);
				if (endPos != string::npos)
				{
					embeddedUrl = resultUrl.substr(startPos, endPos - startPos);
				}
				else
				{
					embeddedUrl = resultUrl.substr(startPos);
				}

				resultUrl = Url::unescapeUrl(embeddedUrl);
			}
#ifdef DEBUG
			else cout << "SearchEngineInterface::processResult: no embedded URL" << endl;
#endif
		}
#ifdef DEBUG
		else cout << "SearchEngineInterface::processResult: no protocol" << endl;
#endif
	}

	// Remove trailing spaces at the end of the URL
	string trimmedUrl(resultUrl);
	string::size_type pos = trimmedUrl.find_last_of(" ");
	while (pos != string::npos)
	{
		int len = trimmedUrl.length();
#ifdef DEBUG
		cout << "SearchEngineInterface::processResult: trimming space at " << pos << endl;
#endif
		if (pos == len - 1)
		{
			trimmedUrl.resize(pos);
			pos = trimmedUrl.find_last_of(" ");
		}
		else
		{
			break;
		}
	}
	resultUrl = Url::canonicalizeUrl(trimmedUrl);

	return true;
}
