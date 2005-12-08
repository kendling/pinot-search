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

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <fstream>

#include "HtmlTokenizer.h"
#include "Url.h"
#include "DownloaderFactory.h"

using namespace std;

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		cerr << "Usage: <downloader name> <URL> [STRIP]" << endl;
		return EXIT_FAILURE;
	}

	string downloaderName = argv[1];
	
	Url thisUrl(argv[2]);
	cout << "Protocol: " << thisUrl.getProtocol() << endl;
	cout << "User: " << thisUrl.getUser() << endl;
	cout << "Password: " << thisUrl.getPassword() << endl;
	cout << "Host: " << thisUrl.getHost() << endl;
	cout << "Location: " << thisUrl.getLocation() << endl;
	cout << "File: " << thisUrl.getFile() << endl;
	cout << "Parameters: " << thisUrl.getParameters() << endl;

	if (downloaderName == "-")
	{
		// Don't go further
		return EXIT_SUCCESS;
	}

	// Which Downloader ?
	DownloaderInterface *myDownloader = DownloaderFactory::getDownloader(thisUrl.getProtocol(),
		downloaderName);
	if (myDownloader == NULL)
	{
		cerr << "Couldn't obtain downloader instance (" << thisUrl.getProtocol() << "," << downloaderName << ")" << endl;
		return EXIT_FAILURE;
	}

	unsigned int urlContentLen;
	string contentType;
	DocumentInfo docInfo("Test", argv[2], "", "");
	Document *urlDoc = myDownloader->retrieveUrl(docInfo);
	if (urlDoc == NULL)
	{
		cerr << "Download operation failed !" << endl;
	}
	else
	{
		cout << "Document type is " << urlDoc->getType() << endl;

		unsigned int urlContentLen;
		const char *urlContent = urlDoc->getData(urlContentLen);

		if ((urlContent != NULL) &&
			(urlContentLen > 0))
		{
			// Save the content to a file
			string fileName = thisUrl.getFile();
			if (fileName.empty() == true)
			{
				fileName = "index.html";
			}

			cout << "Saving " << urlContentLen << " bytes to " << fileName << endl;

			ofstream outputFile(fileName.c_str());
			//outputFile.open(fileName.c_str(), ofstream::out|ofstream::trunc);
			// Strip tags ?
			if ((argc >= 3) &&
				(strncasecmp(argv[2], "STRIP", 5) == 0))
			{
				outputFile << HtmlTokenizer::stripTags(urlContent);
			}
			else
			{
				outputFile.write(urlContent, urlContentLen);
			}
			outputFile.close();
		}
		else
		{
			cout << "Document is empty" << endl;
		}

		delete urlDoc;
	}

	delete myDownloader;

	return EXIT_SUCCESS;
}
