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
#include <glib.h>
#include <string>
#include <vector>
#include <iostream>

#include "StringManip.h"
#include "Url.h"
#include "HtmlTokenizer.h"
#include "DownloaderFactory.h"
#include "WebEngine.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::set;
using std::vector;

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

void WebEngine::setHostNameFilter(const string &filter)
{
	m_hostFilter = filter;
}

void WebEngine::setFileNameFilter(const string &filter)
{
	m_fileFilter = filter;
}

void WebEngine::setQuery(const string &queryString)
{
	if (queryString.empty() == true)
	{
		return;
	}

	Document doc;
	doc.setData(queryString.c_str(), queryString.length());
	Tokenizer tokens(&doc);

	m_queryTerms.clear();

	string token;
	while (tokens.nextToken(token) == true)
	{
		// Lower case the word
		m_queryTerms.insert(StringManip::toLowerCase(token));
	}
}

bool WebEngine::processResult(const string &queryUrl, Result &result)
{
	Url queryUrlObj(queryUrl);
	string resultUrl(result.getLocation());
	string queryHost(Url::reduceHost(queryUrlObj.getHost(), 2));

	if (resultUrl.empty() == true)
	{
		return false;
	}

	if ((resultUrl[0] == '/') ||
		((resultUrl.length() > 1) &&
		(resultUrl[0] == '.') &&
		(resultUrl[1] == '/')))
	{
		string fullResultUrl(queryUrlObj.getProtocol());

		fullResultUrl += "://";
		fullResultUrl += queryUrlObj.getHost();
		if (resultUrl[0] == '.')
		{
			fullResultUrl += resultUrl.substr(1);
		}
		else
		{
			fullResultUrl += resultUrl;
		}

		resultUrl = fullResultUrl;
	}

	Url resultUrlObj(resultUrl);

	if ((m_hostFilter.empty() == false) &&
		(resultUrlObj.getHost() != m_hostFilter))
	{
#ifdef DEBUG
		cout << "WebEngine::processResult: skipping " << resultUrl << endl;
#endif
		return false;
	}

	if ((m_fileFilter.empty() == false) &&
		(resultUrlObj.getFile() != m_fileFilter))
	{
#ifdef DEBUG
		cout << "WebEngine::processResult: skipping " << resultUrl << endl;
#endif
		return false;
	}

	// Is the result's host name the same as the search engine's ?
	// FIXME: not all TLDs have leafs at level 2
	if (queryHost == Url::reduceHost(resultUrlObj.getHost(), 2))
	{
		string protocol(resultUrlObj.getProtocol());

		if (protocol.empty() == false)
		{
			string embeddedUrl;

			string::size_type startPos = resultUrl.find(protocol, protocol.length());
			if (startPos != string::npos)
			{
				string::size_type endPos = resultUrl.find("&amp;", startPos);
				if (endPos != string::npos)
				{
					embeddedUrl = resultUrl.substr(startPos, endPos - startPos);
				}
				else
				{
					embeddedUrl = resultUrl.substr(startPos);
				}

				resultUrl = Url::unescapeUrl(embeddedUrl);
			}
#ifdef DEBUG
			else cout << "WebEngine::processResult: no embedded URL" << endl;
#endif
		}
#ifdef DEBUG
		else cout << "WebEngine::processResult: no protocol" << endl;
#endif
	}

	// Trim spaces
	string trimmedUrl(resultUrl);
	StringManip::trimSpaces(trimmedUrl);

	// Make the URL canonical
	result.setLocation(Url::canonicalizeUrl(trimmedUrl));

	// Scan the extract for query terms
	string extract(result.getExtract());
	if (extract.empty() == true)
	{
		return true;
	}
#ifdef DEBUG
	cout << "WebEngine::processResult: " << extract << endl;
#endif

	Document doc;
	doc.setData(extract.c_str(), extract.length());
	Tokenizer tokens(&doc);

	extract.clear();

	string token;
	while (tokens.nextToken(token) == true)
	{
		char *pEscToken = g_markup_escape_text(token.c_str(), -1);
		if (pEscToken == NULL)
		{
			continue;
		}

		// Is this a query term ?
		if (m_queryTerms.find(StringManip::toLowerCase(token)) == m_queryTerms.end())
		{
			extract += pEscToken;
		}
		else
		{
			extract += "<b>";
			extract += pEscToken;
			extract += "</b>";
		}
		extract += " ";

		g_free(pEscToken);
#ifdef DEBUG
		cout << "WebEngine::processResult: " << extract << endl;
#endif

		result.setExtract(extract);
	}

	return true;
}
