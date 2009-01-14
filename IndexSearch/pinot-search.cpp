/*
 *  Copyright 2005-2009 Fabrice Colin
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
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

#include "config.h"
#include "Languages.h"
#include "MIMEScanner.h"
#include "Url.h"
#include "DownloaderFactory.h"
#include "ModuleFactory.h"
#include "ResultsExporter.h"
#include "WebEngine.h"

using namespace std;

static struct option g_longOptions[] = {
	{"datefirst", 0, 0, 'd'},
	{"help", 0, 0, 'h'},
	{"locationonly", 0, 0, 'l'},
	{"max", 1, 0, 'm'},
	{"proxyaddress", 1, 0, 'a'},
	{"proxyport", 1, 0, 'p'},
	{"proxytype", 1, 0, 't'},
	{"seteditable", 1, 0, 'e'},
	{"stemming", 1, 0, 's'},
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
	map<ModuleProperties, bool> engines;

	// Help
	ModuleFactory::loadModules(string(LIBDIR) + string("/pinot/backends"));
	ModuleFactory::getSupportedEngines(engines);
	ModuleFactory::unloadModules();
	cout << "pinot-search - Query search engines from the command-line\n\n"
		<< "Usage: pinot-search [OPTIONS] SEARCHENGINETYPE SEARCHENGINENAME|SEARCHENGINEOPTION QUERYINPUT\n\n"
		<< "Options:\n"
		<< "  -d, --datefirst           sort by date then by relevance\n"
		<< "  -h, --help                display this help and exit\n"
		<< "  -l, --locationonly        only show the location of each result\n"
		<< "  -m, --max                 maximum number of results (default 10)\n"
		<< "  -a, --proxyaddress        proxy address\n"
		<< "  -p, --proxyport           proxy port\n"
		<< "  -t, --proxytype           proxy type (default HTTP, SOCKS4, SOCKS5)\n"
		<< "  -s, --stemming            stemming language (in English)\n"
		<< "  -e, --seteditable         plugin editable parameter, name:value pair\n"
		<< "  -c, --tocsv               file to export results in CSV format to\n"
		<< "  -x, --toxml               file to export results in XML format to\n"
		<< "  -v, --version             output version information and exit\n"
		<< "  -q, --xesamql             query input is a file containing Xesam QL\n"
		<< "  -u, --xesamul             query input is a file containing Xesam UL\n\n"
		<< "Supported search engine types are :";
	for (map<ModuleProperties, bool>::const_iterator engineIter = engines.begin(); engineIter != engines.end(); ++engineIter)
	{
		cout << " '" << engineIter->first.m_name << "'";
	}
	cout << "\n\nExamples:\n"
		<< "pinot-search opensearch " << PREFIX << "/share/pinot/engines/KrustyDescription.xml \"clowns\"\n\n"
		<< "pinot-search --max 20 sherlock --seteditable \"Bozo App ID:1234567890\" " << PREFIX << "/share/pinot/engines/Bozo.src \"clowns\"\n\n"
		<< "pinot-search googleapi mygoogleapikey \"clowns\"\n\n"
		<< "pinot-search xapian ~/.pinot/index \"label:Clowns\"\n\n"
		<< "pinot-search --stemming english xapian somehostname:12345 \"clowning\"\n\n"
		<< "pinot-search --xesamul xapian ~/.pinot/index some_xesamul_query.txt\n\n"
		<< "pinot-search --xesamql xesam - some_xesamql_query.xml\n\n"
		<< "Report bugs to " << PACKAGE_BUGREPORT << endl;
}

int main(int argc, char **argv)
{
	QueryProperties::QueryType queryType = QueryProperties::XAPIAN_QP;
	string engineType, option, csvExport, xmlExport, proxyAddress, proxyPort, proxyType, editableParameter, stemLanguage;
	unsigned int maxResultsCount = 10; 
	int longOptionIndex = 0;
	bool printResults = true;
	bool sortByDate = false;
	bool locationOnly = false;

	// Look at the options
	int optionChar = getopt_long(argc, argv, "a:c:de:hlm:p:qs:t:uvx:", g_longOptions, &longOptionIndex);
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
			case 'd':
				sortByDate = true;
				break;
			case 'e':
				if (optarg != NULL)
				{
					editableParameter = optarg;
				}
				break;
			case 'h':
				printHelp();
				return EXIT_SUCCESS;
			case 'l':
				locationOnly = true;
				break;
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
			case 's':
				if (optarg != NULL)
				{
					stemLanguage = optarg;
				}
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
		optionChar = getopt_long(argc, argv, "a:c:de:hlm:p:qs:t:uvx:", g_longOptions, &longOptionIndex);
	}

	if (argc == 1)
	{
		printHelp();
		return EXIT_SUCCESS;
	}

	if ((argc < 4) ||
		(argc - optind != 3))
	{
		cerr << "Wrong number of parameters" << endl;
		return EXIT_FAILURE;
	}

	MIMEScanner::initialize("", "");
	DownloaderInterface::initialize();
	ModuleFactory::loadModules(string(LIBDIR) + string("/pinot/backends"));

	// Localize language names
	Languages::setIntlName(0, "Unknown");
	Languages::setIntlName(1, "Danish");
	Languages::setIntlName(2, "Dutch");
	Languages::setIntlName(3, "English");
	Languages::setIntlName(4, "Finnish");
	Languages::setIntlName(5, "French");
	Languages::setIntlName(6, "German");
	Languages::setIntlName(7, "Hungarian");
	Languages::setIntlName(8, "Italian");
	Languages::setIntlName(9, "Norwegian");
	Languages::setIntlName(10, "Portuguese");
	Languages::setIntlName(11, "Romanian");
	Languages::setIntlName(12, "Russian");
	Languages::setIntlName(13, "Spanish");
	Languages::setIntlName(14, "Swedish");
	Languages::setIntlName(15, "Turkish");

	engineType = argv[optind];
	option = argv[optind + 1];
	char *pQueryInput = argv[optind + 2];

	// Which SearchEngine ?
	SearchEngineInterface *pEngine = ModuleFactory::getSearchEngine(engineType, option);
	if (pEngine == NULL)
	{
		cerr << "Couldn't obtain search engine instance" << endl;

		DownloaderInterface::shutdown();
		MIMEScanner::shutdown();

		return EXIT_FAILURE;
	}

	// Set up the proxy
	WebEngine *pWebEngine = dynamic_cast<WebEngine *>(pEngine);
	if (pWebEngine != NULL)
	{
		DownloaderInterface *pDownloader = pWebEngine->getDownloader();
		if ((pDownloader != NULL) &&
			(proxyAddress.empty() == false) &&
			(proxyPort.empty() == false))
		{
			pDownloader->setSetting("proxyaddress", proxyAddress);
			pDownloader->setSetting("proxyport", proxyPort);
			pDownloader->setSetting("proxytype", proxyType);
		}

		string::size_type colonPos = editableParameter.find(':');
		if (colonPos != string::npos)
		{
			map<string, string> editableValues;

			editableValues[editableParameter.substr(0, colonPos)] = editableParameter.substr(colonPos + 1);
			pWebEngine->setEditableValues(editableValues);
		}
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
	queryProps.setStemmingLanguage(stemLanguage);
	queryProps.setMaximumResultsCount(maxResultsCount);
	if (sortByDate == true)
	{
		queryProps.setSortOrder(QueryProperties::DATE);
	}

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

				vector<DocumentInfo>::const_iterator resultIter = resultsList.begin();
				while (resultIter != resultsList.end())
				{
					string rawUrl(resultIter->getLocation());

					if (locationOnly == false)
					{
						cout << count << " Location : '" << rawUrl << "'"<< endl;
						cout << count << " Title    : " << resultIter->getTitle() << endl;
						cout << count << " Type     : " << resultIter->getType() << endl;
						cout << count << " Language : " << resultIter->getLanguage() << endl;
						cout << count << " Date     : " << resultIter->getTimestamp() << endl;
						cout << count << " Size     : " << resultIter->getSize() << endl;
						cout << count << " Extract  : " << resultIter->getExtract() << endl;
						cout << count << " Score    : " << resultIter->getScore() << endl;
					}
					else
					{
						cout << rawUrl << endl;
					}
					++count;

					// Next
					++resultIter;
				}
			}
			else
			{
				string engineName(ModuleFactory::getSearchEngineName(engineType, option));

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
			cerr << "No results" << endl;
		}
	}
	else
	{
		cerr << "Couldn't run query on search engine " << engineType << endl;
	}

	delete pEngine;

	ModuleFactory::unloadModules();
	DownloaderInterface::shutdown();
	MIMEScanner::shutdown();

	return EXIT_SUCCESS;
}
