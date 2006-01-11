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
#include <magic.h>

#include "Url.h"
#include "MIMEScanner.h"

using std::string;

MIMEScanner::MIMEScanner()
{
}

string MIMEScanner::scanFileType(const string &fileName)
{
	string::size_type fileExtPos = fileName.find_last_of(".");
	if (fileExtPos != string::npos)
	{
		string fileExt = fileName.substr(fileExtPos);

		if (strncasecmp(fileExt.c_str(), ".txt", 4) == 0)
		{
			return "text/plain";
		}
		else if (strncasecmp(fileExt.c_str(), ".html", 5) == 0)
		{
			return "text/html";
		}
		else if (strncasecmp(fileExt.c_str(), ".xml", 4) == 0)
		{
			return "text/xml";
		}
		else if (strncasecmp(fileExt.c_str(), ".pdf", 4) == 0)
		{
			return "application/pdf";
		}
		else if (strncasecmp(fileExt.c_str(), ".ps", 3) == 0)
		{
			return "application/postscript";
		}
	}

	// Unknown type
	return "";
}

/// Finds out the given file's MIME type.
string MIMEScanner::scanFile(const string &fileName)
{
	if (fileName.empty() == true)
	{
		return "";
	}

	// Does it have an obvious extension ?
	string mimeType = scanFileType(fileName);
	if (mimeType.empty() == false)
	{
		return mimeType;
	}

	// Open
	magic_t magicCookie = magic_open(MAGIC_SYMLINK|MAGIC_MIME);
	if (magicCookie == NULL)
	{
		return "";
	}
	if (magic_load(magicCookie, NULL) == -1)
	{
		magic_close(magicCookie);
		return "";
	}

	const char *type = magic_file(magicCookie, fileName.c_str());
	if (type != NULL)
	{
		mimeType = type;

		// The MIME string might be of the form "mime_type; charset=..."
		string::size_type mimeTypeEnd = mimeType.find(";");
		if (mimeTypeEnd != string::npos)
		{
			mimeType.resize(mimeTypeEnd);
		}
	}

	// Close
	magic_close(magicCookie);

	return mimeType;
}

/// Finds out the given URL's MIME type.
string MIMEScanner::scanUrl(const Url &urlObj)
{
	// Is it a local file ?
	if (urlObj.getProtocol() == "file")
	{
		string fileName = urlObj.getLocation();
		fileName += "/";
		fileName += urlObj.getFile();

		return scanFile(fileName);
	}

	string mimeType = scanFileType(urlObj.getFile());
	if (mimeType.empty() == true)
	{
		mimeType = "text/plain";
	}

	return mimeType;
}
