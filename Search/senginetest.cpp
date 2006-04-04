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

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <string>
#include <fstream>

#include "SearchEngineFactory.h"
#include "DownloaderFactory.h"
#include "Url.h"
#include "HtmlTokenizer.h"
#include "config.h"

using namespace std;

int main(int argc, char **argv)
{
	string type, option;

	if (argc < 5)
	{
		set<string> engines;

		cerr << "Usage: " << argv[0] << " <search engine type> <name>|<key> <query string> <max results count>" << endl;
		cerr << "Example: " << argv[0] << " sherlock " << PREFIX << "/share/pinot/engines/Bozo.src \"clowns\" 10" << endl;

		SearchEngineFactory::getSupportedEngines(engines);
		cerr << "Supported search engine types:";
		for (set<string>::iterator engineIter = engines.begin(); engineIter != engines.end(); ++engineIter)
		{
			cerr << " " << *engineIter;
		}
		cerr << endl;

		return EXIT_FAILURE;
	}

	// Which SearchEngine ?
	type = argv[1];
	option = argv[2];
	SearchEngineInterface *myEngine = SearchEngineFactory::getSearchEngine(type, option);
	if (myEngine == NULL)
	{
		cerr << "Couldn't obtain search engine instance" << endl;
		return EXIT_FAILURE;
	}

	// How many results ?
	unsigned int count = atoi(argv[4]);
	myEngine->setMaxResultsCount(count);

	QueryProperties queryProps("senginetest", argv[3], "", "", "");
	if (myEngine->runQuery(queryProps) == true)
	{
		string resultsPage;

		// Try getting a list of links
		const vector<Result> resultsList = myEngine->getResults();
		if (resultsList.empty() == false)
		{
			unsigned int count = 0;

			cout << "Matching documents are :" << endl;

			vector<Result>::const_iterator resultIter = resultsList.begin();
			while (resultIter != resultsList.end())
			{
				string rawUrl = (*resultIter).getLocation();
				Url thisUrl(rawUrl);

				cout << count << " Raw URL  : '" << rawUrl << "'"<< endl;
				cout << count << " Protocol : " << thisUrl.getProtocol() << endl;
				cout << count << " Host     : " << thisUrl.getHost() << endl;
				cout << count << " Location : " << thisUrl.getLocation() << "/" << thisUrl.getFile() << endl;
				cout << count << " Title    : " << HtmlTokenizer::stripTags((*resultIter).getTitle()) << endl;
				cout << count << " Extract  : " << HtmlTokenizer::stripTags((*resultIter).getExtract()) << endl;
				cout << count << " Score    : " << (*resultIter).getScore() << endl;
				count++;

				// Next
				resultIter++;
			}
		}
		else
		{
			cerr << "Couldn't get a results list !" << endl;
		}
	}
	else
	{
		cerr << "Couldn't run query on search engine " << argv[1] << endl;
	}

	delete myEngine;

	return EXIT_SUCCESS;
}
