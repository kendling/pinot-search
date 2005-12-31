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

#include <iostream>
#include <algorithm>

#include "HtmlDocument.h"
#include "HtmlTokenizer.h"
#include "MIMEScanner.h"
#include "Url.h"
#include "FileCollector.h"

using namespace std;

FileCollector::FileCollector() :
	DownloaderInterface()
{
}

FileCollector::~FileCollector()
{
}

//
// Implementation of DownloaderInterface
//

/// Retrieves the specified document; NULL if error.
Document *FileCollector::retrieveUrl(const DocumentInfo &docInfo)
{
	Document *fileDocument = NULL;
	Url thisUrl(docInfo.getLocation());
	string protocol = thisUrl.getProtocol();

	if (protocol != "file")
	{
		// We can't handle that type of protocol...
		return NULL;
	}

	string directoryName = thisUrl.getLocation();
	string fileName = thisUrl.getFile();
	string fileLocation = directoryName;
	fileLocation += "/";
	fileLocation += fileName;

	// Determine the file type
	string fileType = MIMEScanner::scanFile(fileLocation);
	// Is it an HTML type ?
	if (fileType.find("html") != string::npos)
	{
		fileDocument = new HtmlDocument(docInfo.getTitle(),
			docInfo.getLocation(), fileType, docInfo.getLanguage());
	}
	else
	{
		fileDocument = new Document(docInfo.getTitle(),
			docInfo.getLocation(), fileType, docInfo.getLanguage());
	}
	if (fileDocument->setDataFromFile(fileLocation) == false)
	{
		delete fileDocument;
		return NULL;
	}

	return fileDocument;
}
