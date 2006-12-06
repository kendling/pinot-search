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
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <fstream>

#include "MIMEScanner.h"
#include "Url.h"
#include "DownloaderFactory.h"
#include "config.h"

using namespace std;

static struct option g_longOptions[] = {
	{"help", 0, 0, 'h'},
	{"version", 0, 0, 'v'},
	{0, 0, 0, 0}
};

int main(int argc, char **argv)
{
	int longOptionIndex = 0;

	// Look at the options
	int optionChar = getopt_long(argc, argv, "hv", g_longOptions, &longOptionIndex);
	while (optionChar != -1)
	{
		switch (optionChar)
		{
			case 'h':
				// Help
				cout << "pinot-collect - Download an URL from the command-line\n\n"
					<< "Usage: pinot-collect [OPTIONS] URL\n\n"
					<< "Options:\n"
					<< "  -h, --help		display this help and exit\n"
					<< "  -v, --version		output version information and exit\n"
					<< "\nExamples:\n"
					<< "  pinot-collect http://some.website.com/\n"
					<< "  pinot-collect xapian:///home/fabrice/.pinot/index/1\n"
					<< "\nReport bugs to " << PACKAGE_BUGREPORT << endl;
				return EXIT_SUCCESS;
			case 'v':
				cout << "pinot-collect - " << PACKAGE_STRING << "\n\n"
					<< "This is free software.  You may redistribute copies of it under the terms of\n"
					<< "the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n"
					<< "There is NO WARRANTY, to the extent permitted by law." << endl;
				return EXIT_SUCCESS;
			default:
				return EXIT_FAILURE;
		}

		// Next option
		optionChar = getopt_long(argc, argv, "hv", g_longOptions, &longOptionIndex);
	}

	if (argc < 2)
	{
		cerr << "Not enough parameters" << endl;
		return EXIT_FAILURE;
	}

	MIMEScanner::initialize();
	DownloaderInterface::initialize();

	string url(argv[1]);
	Url thisUrl(url);
	cout << "Protocol: " << thisUrl.getProtocol() << endl;
	cout << "User: " << thisUrl.getUser() << endl;
	cout << "Password: " << thisUrl.getPassword() << endl;
	cout << "Host: " << thisUrl.getHost() << endl;
	cout << "Location: " << thisUrl.getLocation() << endl;
	cout << "File: " << thisUrl.getFile() << endl;
	cout << "Parameters: " << thisUrl.getParameters() << endl;

	// Which Downloader ?
	DownloaderInterface *pDownloader = DownloaderFactory::getDownloader(thisUrl.getProtocol());
	if (pDownloader == NULL)
	{
		cerr << "Couldn't obtain downloader for protocol " << thisUrl.getProtocol() << endl;

		DownloaderInterface::shutdown();
		MIMEScanner::shutdown();

		return EXIT_FAILURE;
	}

	DocumentInfo docInfo("Test", url, "", "");
	Document *pDoc = pDownloader->retrieveUrl(docInfo);
	if (pDoc == NULL)
	{
		cerr << "Download operation failed !" << endl;
	}
	else
	{
		cout << "Type: " << pDoc->getType() << endl;

		unsigned int contentLen;
		const char *pContent = pDoc->getData(contentLen);

		if ((pContent != NULL) &&
			(contentLen > 0))
		{
			string fileName(thisUrl.getFile());

			if (fileName.empty() == true)
			{
				fileName = "index.html";
			}

			cout << "Saving " << contentLen << " bytes to " << fileName << endl;

			// Save the content to a file
			ofstream outputFile(fileName.c_str());
			outputFile.write(pContent, contentLen);
			outputFile.close();
		}
		else
		{
			cout << "Document is empty" << endl;
		}

		delete pDoc;
	}

	delete pDownloader;

	DownloaderInterface::shutdown();
	MIMEScanner::shutdown();

	return EXIT_SUCCESS;
}
