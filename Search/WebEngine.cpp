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

#include <string.h>
#include <sys/types.h>
#include <regex.h>
#include <iostream>
#include <algorithm>

#include "HtmlTokenizer.h"
#include "StringManip.h"
#include "Url.h"
#include "DownloaderFactory.h"
#include "WebEngine.h"

WebEngine::WebEngine() :
	SearchEngineInterface()
{
}

WebEngine::~WebEngine()
{
	m_resultsList.clear();
}

Document *WebEngine::downloadPage(const DocumentInfo &docInfo)
{
	m_charset.clear();

	// Any type of downloader will do...
	DownloaderInterface *myDownloader = DownloaderFactory::getDownloader("http");
	if (myDownloader == NULL)
	{
		return NULL;
	}

	Document *urlDoc = myDownloader->retrieveUrl(docInfo);
	if (urlDoc != NULL)
	{
		string contentType = urlDoc->getType();

		// Was a charset specified ?
		string::size_type pos = contentType.find("charset=");
		if (pos != string::npos)
		{
			m_charset = StringManip::removeQuotes(contentType.substr(pos + 8));
		}
	}
	delete myDownloader;

	return urlDoc;
}
