/*
 *  Copyright 2005,2006,2007 Fabrice Colin
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
#include <fstream>
#include <string>

#include "MIMEScanner.h"
#include "Url.h"
#include "XapianDatabaseFactory.h"
#include "SearchEngineFactory.h"
#include "ResultsExporter.h"
#include "DownloaderFactory.h"
#include "config.h"

using namespace std;

static struct option g_longOptions[] = {
	{"help", 0, 0, 'h'},
	{"max", 1, 0, 'm'},
	{"proxyaddress", 1, 0, 'a'},
	{"proxyport", 1, 0, 'p'},
	{"proxytype", 1, 0, 't'},
	{"tocsv", 1, 0, 'c'},
	{"toxml", 1, 0, 'x'},
	{"version", 0, 0, 'v'},
	{"xesamql", 0, 0, 'q'},
	{"xesamul", 0, 0, 'u'},
	{0, 0, 0, 0}
};

static bool loadFile(const string &xesamFile, string &fileContents)
{
	ifstream inputFile;
	bool readFile = false;

	inputFile.open(xesamFile.c_str());
	if (inputFile.good() == true)
	{
		inputFile.seekg(0, ios::end);
		int length = inputFile.tellg();
		inputFile.seekg(0, ios::beg);

		char *pFileBuffer = new char[length + 1];
		inputFile.read(pFileBuffer, length);
		if (inputFile.fail() == false)
		{
			pFileBuffer[length] = '\0';

			fileContents = string(pFileBuffer, length);
			readFile = true;
		}
		delete[] pFileBuffer;
	}
	inputFile.close();

	return readFile;
}

static void printHelp(void)
{
	set<string> engines;

	// Help
	SearchEngineFactory::getSupportedEngines(engines);
	cout << "pinot-search - Query search engines from the command-line\n\n"
		<< "Usage: pinot-search [OPTIONS] SEARCHENGINETYPE SEARCHENGINENAME|SEARCHENGINEOPTION QUERYINPUT\n\n"
		<< "Options:\n"
		<< "  -h, --help                display this help and exit\n"
		<< "  -m, --max                 maximum number of results (default 10)\n"
		<< "  -a, --proxyaddress        proxy address\n"
		<< "  -p, --proxyport           proxy port\n"
		<< "  -t, --proxytype           proxy type (default HTTP, SOCKS4, SOCKS5)\n"
		<< "  -c, --tocsv               file to export results in CSV format to\n"
		<< "  -x, --toxml               file to export results in XML format to\n"
		<< "  -v, --version             output version information and exit\n"
		<< "  -q, --xesamql             query input is a file containing Xesam QL\n"
		<< "  -u, --xesamul             query input is a file containing Xesam UL\n\n"
		<< "Supported search engine types are";
	for (set<string>::iterator engineIter = engines.begin(); engineIter != engines.end(); ++engineIter)
	{
		cout << " " << *engineIter;
	}
	cout << "\n\nExamples:\n"
#ifdef HAVE_GOOGLEAPI
		<< "pinot-search googleapi mygoogleapikey \"clowns\"\n\n"
#endif
		<< "pinot-search opensearch " << PREFIX << "/share/pinot/engines/KrustyDescription.xml \"clowns\"\n\n"
		<< "pinot-search --max 20 sherlock " << PREFIX << "/share/pinot/engines/Bozo.src \"clowns\"\n\n"
		<< "pinot-search --max 10 xapian ~/.pinot/index \"clowns\"\n\n"
		<< "pinot-search xapian somehostname:12345 \"clowns\" 10\n\n"
		<< "Report bugs to " << PACKAGE_BUGREPORT << endl;
}

int main(int argc, char **argv)
{
	QueryProperties::QueryType queryType = QueryProperties::XAPIAN_QP;
	string engineType, option, csvExport, xmlExport, proxyAddress, proxyPort, proxyType;
	unsigned int maxResultsCount = 10; 
	int longOptionIndex = 0;
	bool printResults = true;

	// Look at the options
	int optionChar = getopt_long(argc, argv, "c:hm:a:p:qt:uvx:", g_longOptions, &longOptionIndex);
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
			case 'c':
				if (optarg != NULL)
				{
					csvExport = optarg;
					printResults = false;
				}
				break;
			case 'h':
				printHelp();
				return EXIT_SUCCESS;
			case 'm':
				if (optarg != NULL)
				{
					maxResultsCount = (unsigned int )atoi(optarg);
				}
				break;
			case 'p':
				if (optarg != NULL)
				{
					proxyPort = optarg;
				}
				break;
			case 'q':
				queryType = QueryProperties::XESAM_QL;
				break;
			case 't':
				if (optarg != NULL)
				{
					proxyType = optarg;
				}
				break;
			case 'u':
				queryType = QueryProperties::XESAM_UL;
				break;
			case 'v':
				cout << "pinot-search - " << PACKAGE_STRING << "\n\n"
					<< "This is free software.  You may redistribute copies of it under the terms of\n"
					<< "the GNU General Public License <http://www.gnu.org/licenses/old-licenses/gpl-2.0.html>.\n"
					<< "There is NO WARRANTY, to the extent permitted by law." << endl;
				return EXIT_SUCCESS;
			case 'x':
				if (optarg != NULL)
				{
					xmlExport = optarg;
					printResults = false;
				}
				break;
			default:
				return EXIT_FAILURE;
		}

		// Next option
		optionChar = getopt_long(argc, argv, "c:hm:a:p:qt:uvx:", g_longOptions, &longOptionIndex);
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

	engineType = argv[optind];
	option = argv[optind + 1];
	char *pQueryInput = argv[optind + 2];

	// Which SearchEngine ?
	SearchEngineInterface *pEngine = SearchEngineFactory::getSearchEngine(engineType, option);
	if (pEngine == NULL)
	{
		cerr << "Couldn't obtain search engine instance" << endl;

		DownloaderInterface::shutdown();
		MIMEScanner::shutdown();

		return EXIT_FAILURE;
	}

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

	// Set the query
	QueryProperties queryProps("pinot-search", "", queryType);
	if (queryType == QueryProperties::XAPIAN_QP)
	{
		queryProps.setFreeQuery(pQueryInput);
	}
	else
	{
		string fileContents;

		// Load the query from file
		if (loadFile(pQueryInput, fileContents) == false)
		{
			cerr << "Couldn't load query from file " << pQueryInput << endl;

			DownloaderInterface::shutdown();
			MIMEScanner::shutdown();

			return EXIT_FAILURE;
		}

		queryProps.setFreeQuery(fileContents);
	}

	queryProps.setMaximumResultsCount(maxResultsCount);
	pEngine->setDefaultOperator(SearchEngineInterface::DEFAULT_OP_AND);
	if (pEngine->runQuery(queryProps) == true)
	{
		string resultsPage;

		// Try getting a list of links
		const vector<DocumentInfo> resultsList = pEngine->getResults();
		if (resultsList.empty() == false)
		{
			if (printResults == true)
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
					cout << count << " Type     : " << resultIter->getType() << endl;
					cout << count << " Language : " << resultIter->getLanguage() << endl;
					cout << count << " Extract  : " << resultIter->getExtract() << endl;
					cout << count << " Score    : " << resultIter->getScore() << endl;
					count++;

					// Next
					resultIter++;
				}
			}
			else
			{
				string engineName(SearchEngineFactory::getSearchEngineName(engineType, option));

				if (csvExport.empty() == false)
				{
					CSVExporter exporter(csvExport, queryProps);

					exporter.exportResults(engineName, maxResultsCount, resultsList);
				}

				if (xmlExport.empty() == false)
				{
					OpenSearchExporter exporter(xmlExport, queryProps);

					exporter.exportResults(engineName, maxResultsCount, resultsList);
				}
			}
		}
		else
		{
			cerr << "Couldn't get a results list !" << endl;
		}
	}
	else
	{
		cerr << "Couldn't run query on search engine " << engineType << endl;
	}

	delete pEngine;

	XapianDatabaseFactory::closeAll();
	DownloaderInterface::shutdown();
	MIMEScanner::shutdown();

	return EXIT_SUCCESS;
}
