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

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <iostream>
#include <string>
#include <fstream>

#include "MIMEScanner.h"
#include "Url.h"
#include "XapianDatabaseFactory.h"
#include "FilterUtils.h"
#include "SearchEngineFactory.h"
#include "DownloaderFactory.h"
#include "config.h"

using namespace std;

static struct option g_longOptions[] = {
	{"help", 0, 0, 'h'},
	{"max", 1, 0, 'm'},
	{"proxyaddress", 1, 0, 'a'},
	{"proxyport", 1, 0, 'p'},
	{"proxytype", 1, 0, 't'},
	{"version", 0, 0, 'v'},
	{0, 0, 0, 0}
};

static void printHelp(void)
{
	set<string> engines;

	// Help
	SearchEngineFactory::getSupportedEngines(engines);
	cout << "pinot-search - Query search engines from the command-line\n\n"
		<< "Usage: pinot-search [OPTIONS] SEARCHENGINETYPE SEARCHENGINENAME|SEARCHENGINEOPTION QUERYSTRING\n\n"
		<< "Options:\n"
		<< "  -h, --help                display this help and exit\n"
		<< "  -m, --max                 maximum number of results (default 10)\n"
		<< "  -a, --proxyaddress        proxy address\n"
		<< "  -p, --proxyport           proxy port\n"
		<< "  -t, --proxytype           proxy type (default HTTP, SOCKS4, SOCKS5)\n"
		<< "  -v, --version             output version information and exit\n\n"
		<< "Supported search engine types are";
	for (set<string>::iterator engineIter = engines.begin(); engineIter != engines.end(); ++engineIter)
	{
		cout << " " << *engineIter;
	}
	cout << "\n\nExamples:\n"
#ifdef HAVE_GOOGLEAPI
		<< "pinot-search googleapi mygoogleapikey \"clowns\" 10\n\n"
#endif
		<< "pinot-search opensearch " << PREFIX << "/share/pinot/engines/KrustyDescription.xml \"clowns\"\n\n"
		<< "pinot-search --max 20 sherlock " << PREFIX << "/share/pinot/engines/Bozo.src \"clowns\"\n\n"
		<< "pinot-search --max 10 xapian ~/.pinot/index \"clowns\"\n\n"
		<< "pinot-search xapian somehostname:12345 \"clowns\" 10\n\n"
		<< "Report bugs to " << PACKAGE_BUGREPORT << endl;
}

int main(int argc, char **argv)
{
	string type, option, proxyAddress, proxyPort, proxyType;
	unsigned int count = 10; 
	int longOptionIndex = 0;

	// Look at the options
	int optionChar = getopt_long(argc, argv, "hm:a:p:t:v", g_longOptions, &longOptionIndex);
	while (optionChar != -1)
	{
		switch (optionChar)
		{
			case 'a':
				if (optarg != NULL)
				{
					proxyAddress = optarg;
				}
				break;
			case 'h':
				printHelp();
				return EXIT_SUCCESS;
			case 'm':
				if (optarg != NULL)
				{
					count = (unsigned int )atoi(optarg);
				}
				break;
			case 'p':
				if (optarg != NULL)
				{
					proxyPort = optarg;
				}
				break;
			case 't':
				if (optarg != NULL)
				{
					proxyType = optarg;
				}
				break;
			case 'v':
				cout << "pinot-search - " << PACKAGE_STRING << "\n\n"
					<< "This is free software.  You may redistribute copies of it under the terms of\n"
					<< "the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n"
					<< "There is NO WARRANTY, to the extent permitted by law." << endl;
				return EXIT_SUCCESS;
			default:
				return EXIT_FAILURE;
		}

		// Next option
		optionChar = getopt_long(argc, argv, "hm:a:p:t:v", g_longOptions, &longOptionIndex);
	}

	if (argc == 1)
	{
		printHelp();
		return EXIT_SUCCESS;
	}

	if ((argc < 4) ||
		(argc - optind != 3))
	{
		cerr << "Not enough parameters" << endl;
		return EXIT_FAILURE;
	}

	MIMEScanner::initialize();
	DownloaderInterface::initialize();

	// Which SearchEngine ?
	type = argv[optind];
	option = argv[optind + 1];
	SearchEngineInterface *pEngine = SearchEngineFactory::getSearchEngine(type, option);
	if (pEngine == NULL)
	{
		cerr << "Couldn't obtain search engine instance" << endl;

		DownloaderInterface::shutdown();
		MIMEScanner::shutdown();

		return EXIT_FAILURE;
	}

	// How many results ?
	pEngine->setMaxResultsCount(count);

	// Set up the proxy
	DownloaderInterface *pDownloader = pEngine->getDownloader();
	if ((pDownloader != NULL) &&
		(proxyAddress.empty() == false) &&
		(proxyPort.empty() == false))
	{
		pDownloader->setSetting("proxyaddress", proxyAddress);
		pDownloader->setSetting("proxyport", proxyPort);
		pDownloader->setSetting("proxytype", proxyType);
	}

	QueryProperties queryProps("senginetest", argv[optind + 2]);
	if (pEngine->runQuery(queryProps) == true)
	{
		string resultsPage;

		// Try getting a list of links
		const vector<DocumentInfo> resultsList = pEngine->getResults();
		if (resultsList.empty() == false)
		{
			unsigned int count = 0;

			cout << "Matching documents are :" << endl;

			vector<DocumentInfo>::const_iterator resultIter = resultsList.begin();
			while (resultIter != resultsList.end())
			{
				string rawUrl(resultIter->getLocation());
				Url thisUrl(rawUrl);

				cout << count << " Raw URL  : '" << rawUrl << "'"<< endl;
				cout << count << " Protocol : " << thisUrl.getProtocol() << endl;
				cout << count << " Host     : " << thisUrl.getHost() << endl;
				cout << count << " Location : " << thisUrl.getLocation() << "/" << thisUrl.getFile() << endl;
				cout << count << " Title    : " << resultIter->getTitle() << endl;
				cout << count << " Extract  : " << FilterUtils::stripMarkup(resultIter->getExtract()) << endl;
				cout << count << " Score    : " << resultIter->getScore() << endl;
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
		cerr << "Couldn't run query on search engine " << type << endl;
	}

	delete pEngine;

	XapianDatabaseFactory::closeAll();
	DownloaderInterface::shutdown();
	MIMEScanner::shutdown();

	return EXIT_SUCCESS;
}
