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
 
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <fstream>

#include "config.h"
#include "NLS.h"
#include "Languages.h"
#include "MIMEScanner.h"
#include "Url.h"
#include "FilterFactory.h"
#include "DownloaderFactory.h"
#include "FilterWrapper.h"
#include "ModuleFactory.h"
#include "PinotSettings.h"

using namespace std;

static struct option g_longOptions[] = {
	{"backend", 1, 0, 'b'},
	{"check", 0, 0, 'c'},
	{"db", 1, 0, 'd'},
	{"help", 0, 0, 'h'},
	{"index", 0, 0, 'i'},
	{"showinfo", 0, 0, 's'},
	{"version", 0, 0, 'v'},
	{0, 0, 0, 0}
};

static void printHelp(void)
{
	map<ModuleProperties, bool> engines;

	// Help
	ModuleFactory::loadModules(string(LIBDIR) + string("/pinot/backends"));
	ModuleFactory::getSupportedEngines(engines);
	cout << "pinot-index - Index documents from the command-line\n\n"
		<< "Usage: pinot-index [OPTIONS] URLS\n\n"
		<< "Options:\n"
		<< "  -b, --backend             name of back-end to use (default " << PinotSettings::getInstance().m_defaultBackend << ")\n"
		<< "  -c, --check               check whether the given URL is in the index\n"
		<< "  -d, --db                  path to, or name of, index to use (mandatory)\n"
		<< "  -h, --help                display this help and exit\n"
		<< "  -i, --index               index the given URL\n"
		<< "  -s, --showinfo            show information about the document\n"
		<< "  -v, --version             output version information and exit\n\n"
		<< "Supported back-ends are :";
	for (map<ModuleProperties, bool>::const_iterator engineIter = engines.begin(); engineIter != engines.end(); ++engineIter)
	{
		if ((engineIter->second == true) &&
			(ModuleFactory::isSupported(engineIter->first.m_name, true) == true))
		{
			cout << " '" << engineIter->first.m_name << "'";
		}
	}
	ModuleFactory::unloadModules();
	cout << "\n\nExamples:\n"
		<< "pinot-index --check --showinfo --backend xapian --db ~/.pinot/daemon file:///home/fabrice/Documents/Bozo.txt\n\n"
		<< "pinot-index --index --db \"My Web Pages\" http://pinot.berlios.de/\n\n"
		<< "Report bugs to " << PACKAGE_BUGREPORT << endl;
}

static void closeAll(void)
{
	// Close everything
	ModuleFactory::unloadModules();
	Dijon::FilterFactory::unloadFilters();
	Dijon::HtmlFilter::shutdown();
	DownloaderInterface::shutdown();
	MIMEScanner::shutdown();
}

int main(int argc, char **argv)
{
	string type, option;
	string backendType, databaseName;
	int longOptionIndex = 0;
	unsigned int docId = 0;
	bool checkDocument = false, indexDocument = false, showInfo = false, success = false;

	// Look at the options
	int optionChar = getopt_long(argc, argv, "b:cd:hisv", g_longOptions, &longOptionIndex);
	while (optionChar != -1)
	{
		set<string> engines;

		switch (optionChar)
		{
			case 'b':
				if (optarg != NULL)
				{
					backendType = optarg;
				}
				break;
			case 'c':
				checkDocument = true;
				break;
			case 'd':
				if (optarg != NULL)
				{
					databaseName = optarg;
				}
				break;
			case 'h':
				printHelp();
				return EXIT_SUCCESS;
			case 'i':
				indexDocument = true;
				break;
			case 's':
				showInfo = true;
				break;
			case 'v':
				cout << "pinot-index - " << PACKAGE_STRING << "\n\n"
					<< "This is free software.  You may redistribute copies of it under the terms of\n"
					<< "the GNU General Public License <http://www.gnu.org/licenses/old-licenses/gpl-2.0.html>.\n"
					<< "There is NO WARRANTY, to the extent permitted by law." << endl;
				return EXIT_SUCCESS;
			default:
				return EXIT_FAILURE;
		}

		// Next option
		optionChar = getopt_long(argc, argv, "b:cd:hisv", g_longOptions, &longOptionIndex);
	}

	if (argc == 1)
	{
		printHelp();
		return EXIT_SUCCESS;
	}

	if ((argc < 2) ||
		(argc - optind == 0))
	{
		cerr << "Not enough parameters" << endl;
		return EXIT_FAILURE;
	}

	if (((indexDocument == false) &&
		(checkDocument == false)) ||
		(databaseName.empty() == true))
	{
		cerr << "Incorrect parameters" << endl;
		return EXIT_FAILURE;
	}

	// This will create the necessary directories on the first run
	PinotSettings &settings = PinotSettings::getInstance();
	string confDirectory(PinotSettings::getConfigurationDirectory());

	if (MIMEScanner::initialize(PinotSettings::getHomeDirectory() + "/.local",
		string(SHARED_MIME_INFO_PREFIX)) == false)
	{
		cerr << "Couldn't load MIME settings" << endl;
	}
	DownloaderInterface::initialize();
	Dijon::HtmlFilter::initialize();
	Dijon::FilterFactory::loadFilters(string(LIBDIR) + string("/pinot/filters"));
	ModuleFactory::loadModules(string(LIBDIR) + string("/pinot/backends"));
	ModuleFactory::loadModules(confDirectory + "/backends");

	// Localize language names
	Languages::setIntlName(0, _("Unknown"));
	Languages::setIntlName(1, _("Danish"));
	Languages::setIntlName(2, _("Dutch"));
	Languages::setIntlName(3, _("English"));
	Languages::setIntlName(4, _("Finnish"));
	Languages::setIntlName(5, _("French"));
	Languages::setIntlName(6, _("German"));
	Languages::setIntlName(7, _("Hungarian"));
	Languages::setIntlName(8, _("Italian"));
	Languages::setIntlName(9, _("Norwegian"));
	Languages::setIntlName(10, _("Portuguese"));
	Languages::setIntlName(11, _("Romanian"));
	Languages::setIntlName(12, _("Russian"));
	Languages::setIntlName(13, _("Spanish"));
	Languages::setIntlName(14, _("Swedish"));
	Languages::setIntlName(15, _("Turkish"));

	// Load the settings
	settings.load(PinotSettings::LOAD_ALL);

	atexit(closeAll);

	if (backendType.empty() == true)
	{
		backendType = settings.m_defaultBackend;
	}

	// Is this a known index name ?
	PinotSettings::IndexProperties indexProps = settings.getIndexPropertiesByName(databaseName);
	if (indexProps.m_name.empty() == true)
	{
		// No, it's not
		indexProps.m_location = databaseName;
	}

	// Make sure the index is open in the correct mode
	bool wasObsoleteFormat = false;
	if (ModuleFactory::openOrCreateIndex(backendType, indexProps.m_location, wasObsoleteFormat, (indexDocument ? false : true)) == false)
	{
		cerr << "Couldn't open index " << indexProps.m_location << endl;

		return EXIT_FAILURE;
	}

	// Get a read-write index of the given type
	IndexInterface *pIndex = ModuleFactory::getIndex(backendType, indexProps.m_location);
	if (pIndex == NULL)
	{
		cerr << "Couldn't obtain index for " << indexProps.m_location << endl;

		return EXIT_FAILURE;
	}

	while (optind < argc)
	{
		string urlParam(argv[optind]);

		if (checkDocument == true)
		{
			if (pIndex->isGood() == true)
			{
				docId = pIndex->hasDocument(urlParam);
				if (docId > 0)
				{
					cout << urlParam << ": document ID " << docId << endl;
					success = true;
				}
			}
		}
		if (indexDocument == true)
		{
			Url thisUrl(urlParam);

			// Which Downloader ?
			DownloaderInterface *pDownloader = DownloaderFactory::getDownloader(thisUrl.getProtocol());
			if (pDownloader == NULL)
			{
				cerr << "Couldn't obtain downloader for protocol " << thisUrl.getProtocol() << endl;

				success = false;

				// Next
				++optind;
				continue;
			}

			// Set up the proxy
			if ((settings.m_proxyEnabled == true) &&
				(settings.m_proxyAddress.empty() == false))
			{
				char portStr[64];

				pDownloader->setSetting("proxyaddress", settings.m_proxyAddress);
				snprintf(portStr, 64, "%u", settings.m_proxyPort);
				pDownloader->setSetting("proxyport", portStr);
				pDownloader->setSetting("proxytype", settings.m_proxyType);
			}

			DocumentInfo docInfo("", urlParam, MIMEScanner::scanUrl(thisUrl), "");
			Document *pDoc = pDownloader->retrieveUrl(docInfo);
			if (pDoc == NULL)
			{
				cerr << "Couldn't download " << urlParam << endl;
			}
			else
			{
				FilterWrapper wrapFilter(pIndex);
				set<string> labels;

				// Update an existing document or add to the index ?
				docId = pIndex->hasDocument(urlParam);
				if (docId > 0)
				{
					// Update the document
					if (wrapFilter.updateDocument(*pDoc, docId) == true)
					{
						success = true;
					}
				}
				else
				{
					// Index the document
					success = wrapFilter.indexDocument(*pDoc, labels, docId);
				}

				if (success == true)
				{
					// Flush the index
					pIndex->flush();
				}

				delete pDoc;
			}

			delete pDownloader;
		}
		if ((showInfo == true) &&
			(docId > 0))
		{
			DocumentInfo docInfo;
			set<string> labels;

			cout << "Index version       : " << pIndex->getMetadata("version") << endl;

			if (pIndex->getDocumentInfo(docId, docInfo) == true)
			{
				cout << "Location : '" << docInfo.getLocation() << "'" << endl;
				cout << "Title    : " << docInfo.getTitle() << endl;
				cout << "Type     : " << docInfo.getType() << endl;
				cout << "Language : " << docInfo.getLanguage() << endl;
				cout << "Date     : " << docInfo.getTimestamp() << endl;
				cout << "Size     : " << docInfo.getSize() << endl;
			}
			if (pIndex->getDocumentLabels(docId, labels) == true)
			{
				cout << "Labels   : ";
				for (set<string>::const_iterator labelIter = labels.begin();
					labelIter != labels.end(); ++labelIter)
				{
					if (labelIter->substr(0, 2) == "X-")
					{
						continue;
					}
					cout << "[" << Url::escapeUrl(*labelIter) << "]";
				}
				cout << endl;
			}

			vector<MIMEAction> typeActions;
			MIMEScanner::getDefaultActions(docInfo.getType(), typeActions);
			for (vector<MIMEAction>::const_iterator actionIter = typeActions.begin();
				actionIter != typeActions.end(); ++actionIter)
			{
				cout << "Action   : '" << actionIter->m_name << "' " << actionIter->m_exec << endl;
			}
		}

		// Next
		++optind;
	}
	delete pIndex;

	// Did whatever operation we carried out succeed ?
	if (success == true)
	{
		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}
