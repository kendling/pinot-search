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

#include "ObjectsSearchAPIEngine.h"
#include "OSAPIbetaSoapBindingProxy.h"
#include "OSAPI.nsmap"

using std::cout;
using std::endl;
using namespace OSAPI;

ObjectsSearchAPIEngine::ObjectsSearchAPIEngine() :
	SearchEngineInterface()
{
	// SearchEngineInterface members
	m_maxResultsCount = 10;
}

ObjectsSearchAPIEngine::~ObjectsSearchAPIEngine()
{
	m_resultsList.clear();
}

bool ObjectsSearchAPIEngine::makeCall(const string &query)
{
	if (query.empty() == true)
	{
		return false;
	}

	betaSoapBinding soapProxy;

	struct osapi2__doSearchResponse searchResults;
	searchResults._doSearchReturn = NULL;
	cout << "ObjectsSearchAPIEngine::makeCall: query is " << query << endl;

	int status = soapProxy.osapi2__doSearch(query, 0, 10, searchResults);
	cout << "ObjectsSearchAPIEngine::makeCall: doSearch returned " << status << endl;
	if (status == 0)
	{
		if (searchResults._doSearchReturn != NULL)
		{
			cout << "ObjectsSearchAPIEngine::makeCall: results !" << endl;

			osapi1__OS_USCORESearchResult *pResult = searchResults._doSearchReturn;
			unsigned int count = pResult->endIndex - pResult->startIndex;
			cout << "ObjectsSearchAPIEngine::makeCall: " << count << " results out of " << pResult->estimatedTotalResultsCount << endl;

			if ((pResult->resultElements != NULL) &&
				(pResult->resultElements->__size > 0))
			{
				unsigned int numElements = (unsigned int)pResult->resultElements->__size;
				float pseudoScore = 100;

				cout << "ObjectsSearchAPIEngine::makeCall: array has " << numElements << " elements" << endl;
				for (unsigned int currentElement = 0; currentElement < numElements; ++currentElement)
				{
					osapi1__OS_USCORESearchResultElement *pElem = pResult->resultElements->__ptr[currentElement];

					if (pElem != NULL)
					{
						cout << "ObjectsSearchAPIEngine::makeCall: found result" << endl;
						cout << "ObjectsSearchAPIEngine::makeCall: " << *(pElem->URL) << endl;
						cout << "ObjectsSearchAPIEngine::makeCall: " << *(pElem->summary) << endl;
						cout << "ObjectsSearchAPIEngine::makeCall: " << *(pElem->title) << endl;

						string resultUrl(*(pElem->URL));
						if (processResult("http://www.objectssearch.com/", resultUrl) == true)
						{
							m_resultsList.push_back(Result(*(pElem->URL), *(pElem->title), *(pElem->summary), "", pseudoScore));
							--pseudoScore;
						}
					}
				}
			}
		}
		else cout << "ObjectsSearchAPIEngine::makeCall: no results !" << endl;
	}

	return true;
}

//
// Implementation of SearchEngineInterface
//

/// Runs a query; true if success.
bool ObjectsSearchAPIEngine::runQuery(QueryProperties& queryProps)
{
	setHostNameFilter(queryProps.getHostFilter());
	setFileNameFilter(queryProps.getFileFilter());

	// See http://www.objectssearch.com/en/help.html for a description of queries
	string query = queryProps.getAndWords();
	string phrase = queryProps.getPhrase();
	if (phrase.empty() == false)
	{
		query += " \"";
		query += phrase;
		query += "\" ";
	}
	string notWords = queryProps.getNotWords();
	if (notWords.empty() == false)
	{
		query += " -";
		query += notWords;
	}
	// FIXME: not sure what to do about m_anyWords...

	return makeCall(query);
}
