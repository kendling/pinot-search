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
#ifdef HAS_GOOGLEAPI
#include "GoogleAPIEngine.h"
#endif
#include "DownloaderFactory.h"
#include "Url.h"
#include "HtmlTokenizer.h"

using namespace std;

static void fetchCachedPage(const string &url, const string &file, const string &key)
{
#ifdef DEBUG
	cout << "fetchCachedPage: attempting to save " << url << " to " << file << endl;
#endif

#ifdef HAS_GOOGLEAPI
	Url thisUrl(url);
	GoogleAPIEngine googleApiEngine;
	googleApiEngine.setKey(key);

	Document *urlDoc = googleApiEngine.retrieveCachedUrl(url);
	if (urlDoc != NULL)
	{
		unsigned int urlContentLen;
		ofstream outputFile;
		outputFile.open(file.c_str(), ofstream::out | ofstream::trunc);
		outputFile << urlDoc->getData(urlContentLen);
		outputFile.close();

		delete urlDoc;
	}
	else
	{
		cerr << "fetchCachedPage: couldn't get " << url << " !" << endl;
	}
#endif
}

static void fetchPage(const string &url, const string &file)
{
#ifdef DEBUG
	cout << "fetchPage: attempting to save " << url << " to " << file << endl;
#endif

	// Any type of downloader will do...
	Url thisUrl(url);
	DownloaderInterface *myDownloader = DownloaderFactory::getDownloader(thisUrl.getProtocol(), "");
	if (myDownloader == NULL)
	{
		cerr << "fetchPage: couldn't obtain downloader instance (" << thisUrl.getProtocol() << ")" << endl;
		return;
	}

	DocumentInfo docInfo("Page", url, "", "");
	Document *urlDoc = myDownloader->retrieveUrl(docInfo);
	if (urlDoc != NULL)
	{
		unsigned int urlContentLen;
		ofstream outputFile;
		outputFile.open(file.c_str(), ofstream::out | ofstream::trunc);
		outputFile << urlDoc->getData(urlContentLen);
		outputFile.close();

		delete urlDoc;
	}
	else
	{
		cerr << "fetchPage: couldn't get " << url << " !" << endl;
	}

	delete myDownloader;
}

int main(int argc, char **argv)
{
	string type, option;
	bool bDownloadResults = false;

	if (argc < 5)
	{
		cerr << "Usage: " << argv[0] << " <search engine name> <option> <search string> <max results> [DOWNLOAD]" << endl;
		return EXIT_FAILURE;
	}
	if (argc > 5)
	{
		string flag = argv[5];
		if (flag == "DOWNLOAD")
		{
			bDownloadResults = true;
		}
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
	bool bOK = myEngine->runQuery(queryProps);
	if (bOK == true)
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

				if (bDownloadResults == true)
				{
					// Set the name of the file to which this page will be saved
					char num[16];
					sprintf(num, "%d", count);
					string url = (*resultIter).getLocation();
					string file = num;
					file += "_";
					file += thisUrl.getHost();
					file += ".html";

					if (type == "googleapi")
					{
						// Fetch the page from the Google cache
						fetchCachedPage(url, file, option);
					}
					else
					{
						fetchPage(url, file);
					}
				}
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
