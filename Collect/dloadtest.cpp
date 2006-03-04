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

#include "Url.h"
#include "DownloaderFactory.h"

using namespace std;

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		cerr << "Usage: <URL>" << endl;
		return EXIT_FAILURE;
	}

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
	DownloaderInterface *myDownloader = DownloaderFactory::getDownloader(thisUrl.getProtocol());
	if (myDownloader == NULL)
	{
		DownloaderInterface::shutdown();
		cerr << "Couldn't obtain downloader for protocol " << thisUrl.getProtocol() << endl;
		return EXIT_FAILURE;
	}

	unsigned int urlContentLen;
	string contentType;
	DocumentInfo docInfo("Test", url, "", "");
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
			outputFile.write(urlContent, urlContentLen);
			outputFile.close();
		}
		else
		{
			cout << "Document is empty" << endl;
		}

		delete urlDoc;
	}

	delete myDownloader;

	DownloaderInterface::shutdown();

	return EXIT_SUCCESS;
}
