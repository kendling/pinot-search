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

#include <string.h>
#include <sys/types.h>
#include <regex.h>
#include <string>

#include "StringManip.h"
#include "Url.h"
#include "HtmlTokenizer.h"
#include "DownloaderFactory.h"
#include "WebEngine.h"

using std::string;

static string getCharset(const string &contentType)
{
	if (contentType.empty() == false)
	{
		// Is a charset specified ?
		string::size_type pos = contentType.find("charset=");
		if (pos != string::npos)
		{
			return StringManip::removeQuotes(contentType.substr(pos + 8));
		}
	}

	return "";
}

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
	DownloaderInterface *pDownloader = DownloaderFactory::getDownloader("http");
	if (pDownloader == NULL)
	{
		return NULL;
	}

	Document *pDoc = pDownloader->retrieveUrl(docInfo);
	if (pDoc != NULL)
	{
		string contentType(pDoc->getType());

		// Found a charset ?
		m_charset = getCharset(contentType);
		if (m_charset.empty() == true)
		{
			HtmlTokenizer tokens(pDoc, true);

			// Content-Type might be specified as a META tag 
			contentType = tokens.getMetaTag("Content-Type");
			m_charset = getCharset(contentType);
			if (m_charset.empty() == false)
			{
				// Reset the document's type
				pDoc->setType(contentType);
			}
		}
	}
	delete pDownloader;

	return pDoc;
}
