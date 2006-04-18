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

#include <strings.h>
#include <utility>
#include <iostream>

#include "xdgmime/xdgmime.h"

#include "MIMEScanner.h"
#include "StringManip.h"
#include "Url.h"

using std::cout;
using std::endl;
using std::string;
using std::min;

MIMEScanner::MIMEScanner()
{
}

string MIMEScanner::scanFileType(const string &fileName)
{
	if (fileName.empty() == true)
	{
		return "";
	}

	// Does it have an obvious extension ?
	const char  *pType = xdg_mime_get_mime_type_from_file_name(fileName.c_str());
	if ((pType == NULL) ||
		(strncasecmp(pType, xdg_mime_type_unknown, min(strlen(pType), strlen(xdg_mime_type_unknown))) == 0))
	{
#ifdef DEBUG
		cout << "MIMEScanner::scanFileType: couldn't determine type of " << fileName << endl;
#endif
		return "";
	}

	return pType;
}

/// Finds out the given file's MIME type.
string MIMEScanner::scanFile(const string &fileName)
{
	if (fileName.empty() == true)
	{
		return "";
	}

	string mimeType = scanFileType(fileName);
	if (mimeType.empty() == true)
	{
		// Have a peek at the file
		const char *pType = xdg_mime_get_mime_type_for_file(fileName.c_str(), NULL);
		if ((pType != NULL) &&
			(strncasecmp(pType, xdg_mime_type_unknown, min(strlen(pType), strlen(xdg_mime_type_unknown))) != 0))
		{
			return pType;
		}

#ifdef DEBUG
		cout << "MIMEScanner::scanFile: couldn't determine type of " << fileName << endl;
#endif
		if (xdg_mime_type_unknown != NULL)
		{
			mimeType = xdg_mime_type_unknown;
		}
	}

	return mimeType;
}

/// Finds out the given URL's MIME type.
string MIMEScanner::scanUrl(const Url &urlObj)
{
	string mimeType = scanFileType(urlObj.getFile());
	if (mimeType.empty() == true)
	{
		// Is it a local file ?
		if (urlObj.getProtocol() == "file")
		{
			string fileName = urlObj.getLocation();
			fileName += "/";
			fileName += urlObj.getFile();

			mimeType = scanFile(fileName);
		}
	}

	if (mimeType.empty() == true)
	{
		if (urlObj.getProtocol() == "http")
		{
			mimeType = "text/html";
		}
		else if (xdg_mime_type_unknown != NULL)
		{
			mimeType = xdg_mime_type_unknown;
		}
	}

	return mimeType;
}
