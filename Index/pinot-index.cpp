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
	{"check", 1, 0, 'c'},
	{"db", 1, 0, 'd'},
	{"help", 0, 0, 'h'},
	{"index", 1, 0, 'i'},
	{"showinfo", 0, 0, 's'},
	{"version", 0, 0, 'v'},
	{0, 0, 0, 0}
};

int main(int argc, char **argv)
{
	string type, option;
	string databaseName, urlToCheck, urlToIndex;
	int longOptionIndex = 0;
	unsigned int docId = 0;
	bool checkDocument = false, indexDocument = false, showInfo = false, success = false;

	// Look at the options
	int optionChar = getopt_long(argc, argv, "c:d:hi:sv", g_longOptions, &longOptionIndex);
	while (optionChar != -1)
	{
		set<string> engines;

		switch (optionChar)
		{
			case 'c':
				if (optarg != NULL)
				{
					urlToCheck = optarg;
				}
				checkDocument = true;
				break;
			case 'd':
				if (optarg != NULL)
				{
					databaseName = optarg;
				}
				checkDocument = true;
				break;
			case 'h':
				// Help
				cout << "pinot-index - Index documents from the command-line\n\n"
					<< "Usage: pinot-index [OPTIONS]\n\n"
					<< "Options:\n"
					<< "  -c, --check               check whether the given URL is in the index\n"
					<< "  -d, --db                  path to index to use (mandatory)\n"
					<< "  -h, --help                display this help and exit\n"
					<< "  -i, --index               index the given URL\n"
					<< "  -s, --showinfo            show information about the document\n"
					<< "  -v, --version             output version information and exit\n\n";
				// Don't mention type dbus here as it doesn't support indexing and
				// is identical to xapian when checking for URLs
				cout << "Examples:\n"
					<< "pinot-index --check file:///home/fabrice/Documents/Bozo.txt --showinfo --db ~/.pinot/daemon\n\n"
					<< "pinot-index --index http://pinot.berlios.de/ --db ~/.pinot/index\n\n"
					<< "Report bugs to " << PACKAGE_BUGREPORT << endl;
				return EXIT_SUCCESS;
			case 'i':
				if (optarg != NULL)
				{
					urlToIndex = optarg;
				}
				indexDocument = true;
				break;
			case 's':
				showInfo = true;
				break;
			case 'v':
				cout << "pinot-index - " << PACKAGE_STRING << "\n\n"
					<< "This is free software.  You may redistribute copies of it under the terms of\n"
					<< "the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n"
					<< "There is NO WARRANTY, to the extent permitted by law." << endl;
				return EXIT_SUCCESS;
			default:
				return EXIT_FAILURE;
		}

		// Next option
		optionChar = getopt_long(argc, argv, "c:d:hi:sv", g_longOptions, &longOptionIndex);
	}

	if (((indexDocument == false) &&
		(checkDocument == false)) ||
		(databaseName.empty() == true))
	{
		cerr << "Incorrect parameters" << endl;
		return EXIT_FAILURE;
	}

	MIMEScanner::initialize();
	DownloaderInterface::initialize();
	Dijon::FilterFactory::loadFilters(string(LIBDIR) + string("/pinot/filters"));

	// Make sure the index is open in the correct mode
	XapianDatabase *pDb = XapianDatabaseFactory::getDatabase(databaseName, (indexDocument ? false : true));
	if (pDb == NULL)
	{
		cerr << "Couldn't open index " << databaseName << endl;

		Dijon::FilterFactory::unloadFilters();
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
		DownloaderInterface::shutdown();
		MIMEScanner::shutdown();

		return EXIT_FAILURE;
	}

	if (checkDocument == true)
	{
		if (pIndex->isGood() == true)
		{
			docId = pIndex->hasDocument(urlToCheck);
			if (docId > 0)
			{
				cout << urlToCheck << ": document ID " << docId << endl;
				success = true;
			}
		}
	}
	if (indexDocument == true)
	{
		Url thisUrl(urlToIndex);

		// Which Downloader ?
		DownloaderInterface *pDownloader = DownloaderFactory::getDownloader(thisUrl.getProtocol());
		if (pDownloader == NULL)
		{
			cerr << "Couldn't obtain downloader for protocol " << thisUrl.getProtocol() << endl;

			XapianDatabaseFactory::closeAll();
			Dijon::FilterFactory::unloadFilters();
			DownloaderInterface::shutdown();
			MIMEScanner::shutdown();

			return EXIT_FAILURE;
		}

		DocumentInfo docInfo(urlToIndex, urlToIndex, MIMEScanner::scanUrl(thisUrl), "");
		Document *pDoc = pDownloader->retrieveUrl(docInfo);
		if (pDoc == NULL)
		{
			cerr << "Download operation failed !" << endl;
		}
		else
		{
			set<string> labels;

			pIndex->setStemmingMode(IndexInterface::STORE_BOTH);

			// Update an existing document or add to the index ?
			docId = pIndex->hasDocument(urlToIndex);
			if (docId > 0)
			{
				// Update the document
				if (FilterWrapper::updateDocument(docId, *pIndex, *pDoc) == true)
				{
					success = true;
				}
			}
			else
			{
				// Index the document
				success = FilterWrapper::indexDocument(*pIndex, *pDoc, labels, docId);
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
	DownloaderInterface::shutdown();
	MIMEScanner::shutdown();

	// Did whatever operation we carried out succeed ?
	if (success == true)
	{
		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}
