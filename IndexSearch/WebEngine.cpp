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
#include "HtmlFilter.h"
#include "DownloaderFactory.h"
#include "FilterUtils.h"
#include "CJKVTokenizer.h"
#include "WebEngine.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::set;
using std::vector;

class TermHighlighter : public Dijon::CJKVTokenizer::TokensHandler
{
        public:
                TermHighlighter(string &extract, set<string> &queryTerms,
			unsigned int nGramSize) :
			Dijon::CJKVTokenizer::TokensHandler(),
			m_extract(extract),
			m_nGramSize(nGramSize),
			m_nGramCount(0),
			m_queryTerms(queryTerms)
		{
		}

		virtual ~TermHighlighter()
		{
		}

		virtual bool handle_token(const string &tok, bool is_cjkv)
		{
			gchar *pEscToken = NULL;
			gchar *pUTF8Token = NULL;
			gsize bytesWritten = 0;

			if (tok.empty() == true)
			{
				return false;
			}

			if (is_cjkv == false)
			{
				m_nGramCount = 0;
			}
			else
			{
				++m_nGramCount;
				if (tok.length() > 4)
				{
					// Ignore multi-character tokens
#ifdef DEBUG
					cout << "WebEngine::processResult: ignoring " << tok << endl;
#endif
					return true;
				}
			}

			pUTF8Token = g_locale_to_utf8(tok.c_str(), tok.length(),
				NULL, &bytesWritten, NULL);
			if (pUTF8Token != NULL)
			{
				pEscToken = g_markup_escape_text(pUTF8Token, -1);
				g_free(pUTF8Token);
			}
			if (pEscToken == NULL)
			{
				return true;
			}

			if ((m_extract.empty() == false) &&
				(m_nGramCount <= 1))
			{
				m_extract += " ";
			}
			// Is this a query term ?
			if (m_queryTerms.find(StringManip::toLowerCase(tok)) == m_queryTerms.end())
			{
				m_extract += pEscToken;
			}
			else
			{
				m_extract += "<b>";
				m_extract += pEscToken;
				m_extract += "</b>";
			}

			g_free(pEscToken);
		}

	protected:
		string &m_extract;
		unsigned int m_nGramSize;
		unsigned int m_nGramCount;
		set<string> &m_queryTerms;

};

WebEngine::WebEngine() :
	SearchEngineInterface(),
	m_pDownloader(DownloaderFactory::getDownloader("http"))
{
}

WebEngine::~WebEngine()
{
	if (m_pDownloader != NULL)
	{
		delete m_pDownloader;
	}
}

Document *WebEngine::downloadPage(const DocumentInfo &docInfo)
{
	m_charset.clear();

	if (m_pDownloader == NULL)
	{
		return NULL;
	}

	Document *pDoc = m_pDownloader->retrieveUrl(docInfo);
	if (pDoc != NULL)
	{
		string contentType(pDoc->getType());

		// Is a charset specified ?
		string::size_type pos = contentType.find("charset=");
		if (pos != string::npos)
		{
			m_charset = StringManip::removeQuotes(contentType.substr(pos + 8));
		}
		if (m_charset.empty() == true)
		{
			Dijon::HtmlFilter htmlFilter("text/html");

			if (FilterUtils::feedFilter(*pDoc, &htmlFilter) == true)
			{
				const map<string, string> &metaData = htmlFilter.get_meta_data();
				map<string, string>::const_iterator charsetIter = metaData.find("charset");

				if (charsetIter != metaData.end())
				{
					m_charset = charsetIter->second;
				}
			}
		}
#ifdef DEBUG
		cout << "WebEngine::downloadPage: charset is " << m_charset << endl;
#endif
	}

	return pDoc;
}

void WebEngine::setQuery(const QueryProperties &queryProps)
{
	queryProps.getTerms(m_queryTerms);
}

bool WebEngine::processResult(const string &queryUrl, DocumentInfo &result)
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

	Dijon::CJKVTokenizer tokenizer;
	TermHighlighter handler(extract, m_queryTerms, tokenizer.get_ngram_size());

	// Highlight query terms in the extract
	extract.clear();
	tokenizer.tokenize(result.getExtract(), handler);
	result.setExtract(extract);

	return true;
}

/// Returns the downloader used if any.
DownloaderInterface *WebEngine::getDownloader(void)
{
	return m_pDownloader;
}
