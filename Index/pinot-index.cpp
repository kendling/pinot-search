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
 
#include <getopt.h>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <string>
#include <fstream>

#include "MIMEScanner.h"
#include "Url.h"
#include "FilterFactory.h"
#include "XapianDatabaseFactory.h"
#include "DownloaderFactory.h"
#include "FilterWrapper.h"
#include "IndexFactory.h"
#include "config.h"

using namespace std;

static struct option g_longOptions[] = {
	{"check", 0, 0, 'c'},
	{"db", 1, 0, 'd'},
	{"help", 0, 0, 'h'},
	{"index", 0, 0, 'i'},
	{"proxyaddress", 1, 0, 'a'},
	{"proxyport", 1, 0, 'p'},
	{"proxytype", 1, 0, 't'},
	{"showinfo", 0, 0, 's'},
	{"version", 0, 0, 'v'},
	{0, 0, 0, 0}
};

static void printHelp(void)
{
	// Help
	cout << "pinot-index - Index documents from the command-line\n\n"
		<< "Usage: pinot-index [OPTIONS] URL\n\n"
		<< "Options:\n"
		<< "  -c, --check               check whether the given URL is in the index\n"
		<< "  -d, --db                  path to index to use (mandatory)\n"
		<< "  -h, --help                display this help and exit\n"
		<< "  -i, --index               index the given URL\n"
		<< "  -a, --proxyaddress        proxy address\n"
		<< "  -p, --proxyport           proxy port\n"
		<< "  -t, --proxytype           proxy type (default HTTP, SOCKS4, SOCKS5)\n"
		<< "  -s, --showinfo            show information about the document\n"
		<< "  -v, --version             output version information and exit\n\n";
	// Don't mention type dbus here as it doesn't support indexing and
	// is identical to xapian when checking for URLs
	cout << "Examples:\n"
		<< "pinot-index --check --showinfo --db ~/.pinot/daemon file:///home/fabrice/Documents/Bozo.txt\n\n"
		<< "pinot-index --index --db ~/.pinot/index http://pinot.berlios.de/\n\n"
		<< "Report bugs to " << PACKAGE_BUGREPORT << endl;
}

int main(int argc, char **argv)
{
	string type, option;
	string databaseName, proxyAddress, proxyPort, proxyType;
	int longOptionIndex = 0;
	unsigned int docId = 0;
	bool checkDocument = false, indexDocument = false, showInfo = false, success = false;

	// Look at the options
	int optionChar = getopt_long(argc, argv, "cd:hia:p:t:sv", g_longOptions, &longOptionIndex);
	while (optionChar != -1)
	{
		set<string> engines;

		switch (optionChar)
		{
			case 'a':
				if (optarg != NULL)
				{
					proxyAddress = optarg;
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
			case 'p':
				if (optarg != NULL)
				{
					proxyPort = optarg;
				}
				break;
			case 's':
				showInfo = true;
				break;
			case 't':
				if (optarg != NULL)
				{
					proxyType = optarg;
				}
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
		optionChar = getopt_long(argc, argv, "cd:hia:p:t:sv", g_longOptions, &longOptionIndex);
	}

	if (argc == 1)
	{
		printHelp();
		return EXIT_SUCCESS;
	}

	if ((argc < 2) ||
		(argc - optind != 1))
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

	string desktopFilesDirectory(SHARED_MIME_INFO_PREFIX);
	desktopFilesDirectory += "/share/applications/";
	MIMEScanner::initialize(desktopFilesDirectory, desktopFilesDirectory + "mimeinfo.cache");
	DownloaderInterface::initialize();
	Dijon::HtmlFilter::initialize();
	Dijon::FilterFactory::loadFilters(string(LIBDIR) + string("/pinot/filters"));

	// Make sure the index is open in the correct mode
	XapianDatabase *pDb = XapianDatabaseFactory::getDatabase(databaseName, (indexDocument ? false : true));
	if (pDb == NULL)
	{
		cerr << "Couldn't open index " << databaseName << endl;

		Dijon::FilterFactory::unloadFilters();
		Dijon::HtmlFilter::shutdown();
		DownloaderInterface::shutdown();
		MIMEScanner::shutdown();

		return EXIT_FAILURE;
	}

	// Get a read-write index of the given type
	IndexInterface *pIndex = IndexFactory::getIndex("xapian", databaseName);
	if (pIndex == NULL)
	{
		cerr << "Couldn't obtain index for " << databaseName << endl;

		XapianDatabaseFactory::closeAll();
		Dijon::FilterFactory::unloadFilters();
		Dijon::HtmlFilter::shutdown();
		DownloaderInterface::shutdown();
		MIMEScanner::shutdown();

		return EXIT_FAILURE;
	}

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

			XapianDatabaseFactory::closeAll();
			Dijon::FilterFactory::unloadFilters();
			Dijon::HtmlFilter::shutdown();
			DownloaderInterface::shutdown();
			MIMEScanner::shutdown();

			return EXIT_FAILURE;
		}

		// Set up the proxy
		if ((proxyAddress.empty() == false) &&
			(proxyPort.empty() == false))
		{
			pDownloader->setSetting("proxyaddress", proxyAddress);
			pDownloader->setSetting("proxyport", proxyPort);
			pDownloader->setSetting("proxytype", proxyType);
		}

		DocumentInfo docInfo(urlParam, urlParam, MIMEScanner::scanUrl(thisUrl), "");
		Document *pDoc = pDownloader->retrieveUrl(docInfo);
		if (pDoc == NULL)
		{
			cerr << "Download operation failed !" << endl;
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

		if (pIndex->getDocumentInfo(docId, docInfo) == true)
		{
			cout << "Title: " << docInfo.getTitle() << endl;
			cout << "Location: " << docInfo.getLocation() << endl;
			cout << "Type: " << docInfo.getType() << endl;
			cout << "Language: " << docInfo.getLanguage() << endl;
			cout << "Timestamp: " << docInfo.getTimestamp() << endl;
			cout << "Size: " << docInfo.getSize() << endl;
		}
		if (pIndex->getDocumentLabels(docId, labels) == true)
		{
			cout << "Labels:";
			for (set<string>::const_iterator labelIter = labels.begin();
				labelIter != labels.end(); ++labelIter)
			{
				cout << " '" << *labelIter << "'";
			}
			cout << endl;
		}
	}
	delete pIndex;

	XapianDatabaseFactory::closeAll();
	Dijon::FilterFactory::unloadFilters();
	Dijon::HtmlFilter::shutdown();
	DownloaderInterface::shutdown();
	MIMEScanner::shutdown();

	// Did whatever operation we carried out succeed ?
	if (success == true)
	{
		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}
