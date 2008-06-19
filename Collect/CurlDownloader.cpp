/*
 *  Copyright 2005-2008 Fabrice Colin
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <cstdio>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <iostream>
#include <cstdlib>

#include <curl/curl.h>

#include "Url.h"
#include "CurlDownloader.h"

using namespace std;

struct ContentInfo
{
	char *m_pContent;
	size_t m_contentLen;
	string m_lastModified;
};

size_t writeCallback(void *pData, size_t dataSize, size_t elementsCount, void *pStream)
{
	ContentInfo *pInfo = NULL;
	size_t totalSize = elementsCount * dataSize;

	if (pStream == NULL)
	{
		return 0;
	}
	pInfo = (ContentInfo *)pStream;

	char *pNewContent = (char*)realloc(pInfo->m_pContent, pInfo->m_contentLen + totalSize + 1);
	if (pNewContent == NULL)
	{
#ifdef DEBUG
		cout << "writeCallback: failed to enlarge buffer" << endl;
#endif
		if (pInfo->m_pContent != NULL)
		{
			free(pInfo->m_pContent);
			pInfo->m_pContent = NULL;
			pInfo->m_contentLen = 0;
		}
		return 0;
	}

	pInfo->m_pContent = pNewContent;
	memcpy(pInfo->m_pContent + pInfo->m_contentLen, pData, totalSize);
	pInfo->m_contentLen += totalSize;
	pInfo->m_pContent[pInfo->m_contentLen] = '\0';

	if (totalSize < strlen((const char*)pData))
	{
		void *pBadChar = NULL;

		// There's a NULL character in the buffer ? Replace it
		while ((pBadChar = memchr((void*)pInfo->m_pContent, '\0', pInfo->m_contentLen)) != NULL)
		{
			((char*)pBadChar)[0] = ' ';
		}
	}

	return totalSize;
}

size_t headerCallback(void *pData, size_t dataSize, size_t elementsCount, void *pStream)
{
	ContentInfo *pInfo = NULL;
	size_t totalSize = elementsCount * dataSize;

	if ((pStream == NULL) ||
		(pData == NULL) ||
		(totalSize == 0))
	{
		return 0;
	}
	pInfo = (ContentInfo *)pStream;

	string header((const char*)pData, totalSize);
	string::size_type pos = header.find("Last-Modified: ");

	if (pos != string::npos)
	{
		pInfo->m_lastModified = header.substr(15);
#ifdef DEBUG
		cout << "headerCallback: Last-Modified " << pInfo->m_lastModified << endl;
#endif
	}

	return totalSize;
}

unsigned int CurlDownloader::m_initialized = 0;

CurlDownloader::CurlDownloader() :
	DownloaderInterface(),
	m_proxyPort(0)
{
	if (m_initialized == 0)
	{
		// Initialize
		curl_global_init(CURL_GLOBAL_ALL);

		++m_initialized;
	}

        // Pretend to be Mozilla
	m_userAgent = "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.7.3) Gecko/20041020";
}

CurlDownloader::~CurlDownloader()
{
	--m_initialized;
	if (m_initialized == 0)
	{
		// Shutdown
		curl_global_cleanup();
	}
}

//
// Implementation of DownloaderInterface
//

/**
  * Sets a (name, value) setting. Setting names include :
  * proxyaddress - the address of the proxy to use
  * proxyport - the port of the proxy to use (positive integer)
  * proxytype - the type of the proxy to use
  * Returns true if success.
  */
bool CurlDownloader::setSetting(const string &name, const string &value)
{
        bool goodSetting = true;

        if (name == "useragent")
        {
                m_userAgent = value;
        }
	else if (name == "proxyaddress")
	{
		m_proxyAddress = value;
	}
	else if ((name == "proxyport") &&
		(value.empty() == false))
	{
		m_proxyPort = (unsigned int )atoi(value.c_str());
	}
	else if (name == "proxytype")
	{
		m_proxyType = value;
	}
        else
        {
                goodSetting = false;
        }

        return goodSetting;
}

/// Retrieves the specified document; NULL if error.
Document *CurlDownloader::retrieveUrl(const DocumentInfo &docInfo)
{
	Document *pDocument = NULL;
	string url(Url::escapeUrl(docInfo.getLocation()));
	long maxRedirectionsCount = 10;

	if (url.empty() == true)
	{
#ifdef DEBUG
		cout << "CurlDownloader::retrieveUrl: no URL specified !" << endl;
#endif
		return NULL;
	}

	// Create a session
	CURL *pCurlHandler = curl_easy_init();
	if (pCurlHandler != NULL)
	{
		ContentInfo *pContentInfo = new ContentInfo;

		pContentInfo->m_pContent = NULL;
		pContentInfo->m_contentLen = 0;

		curl_easy_setopt(pCurlHandler, CURLOPT_AUTOREFERER, 1);
		curl_easy_setopt(pCurlHandler, CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt(pCurlHandler, CURLOPT_MAXREDIRS, maxRedirectionsCount);
		curl_easy_setopt(pCurlHandler, CURLOPT_USERAGENT, m_userAgent.c_str());
		curl_easy_setopt(pCurlHandler, CURLOPT_NOSIGNAL, (long)1);
		curl_easy_setopt(pCurlHandler, CURLOPT_TIMEOUT, (long)m_timeout);
#ifndef DEBUG
		curl_easy_setopt(pCurlHandler, CURLOPT_NOPROGRESS, 1);
#endif
		curl_easy_setopt(pCurlHandler, CURLOPT_WRITEFUNCTION, writeCallback);
		curl_easy_setopt(pCurlHandler, CURLOPT_WRITEDATA, pContentInfo);
		curl_easy_setopt(pCurlHandler, CURLOPT_HEADERFUNCTION, headerCallback);
		curl_easy_setopt(pCurlHandler, CURLOPT_HEADERDATA, pContentInfo);
#ifdef DEBUG
		cout << "CurlDownloader::retrieveUrl: URL is " << url << endl;
#endif
		curl_easy_setopt(pCurlHandler, CURLOPT_URL, url.c_str());
		// Is a proxy defined ?
		// Curl automatically checks and makes use of the *_proxy environment variables 
		if ((m_proxyAddress.empty() == false) &&
			(m_proxyPort > 0))
		{
			curl_proxytype proxyType = CURLPROXY_HTTP;

			curl_easy_setopt(pCurlHandler, CURLOPT_PROXY, m_proxyAddress.c_str());
			curl_easy_setopt(pCurlHandler, CURLOPT_PROXYPORT, m_proxyPort);
			// Type defaults to HTTP
			if (m_proxyType.empty() == false)
			{
				if (m_proxyType == "SOCKS4")
				{
					proxyType = CURLPROXY_SOCKS4;
				}
				else if (m_proxyType == "SOCKS5")
				{
					proxyType = CURLPROXY_SOCKS5;
				}
			}
			curl_easy_setopt(pCurlHandler, CURLOPT_PROXYTYPE, proxyType);
		}

		CURLcode res = curl_easy_perform(pCurlHandler);
		if (res == CURLE_OK)
		{
			if ((pContentInfo->m_pContent != NULL) &&
				(pContentInfo->m_contentLen > 0))
			{
				char *pContentType = NULL;

				// Copy the document content
				pDocument = new Document(docInfo);
				pDocument->setData(pContentInfo->m_pContent, pContentInfo->m_contentLen);
				pDocument->setLocation(url);
				pDocument->setSize((off_t )pContentInfo->m_contentLen);

				// What's the Content-Type ?
				res = curl_easy_getinfo(pCurlHandler, CURLINFO_CONTENT_TYPE, &pContentType);
				if ((res == CURLE_OK) &&
					(pContentType != NULL))
				{
					pDocument->setType(pContentType);
				}

				// The Last-Modified date ?
				if (pContentInfo->m_lastModified.empty() == false)
				{
					pDocument->setTimestamp(pContentInfo->m_lastModified);
				}
			}
#ifdef DEBUG
			else cout << "CurlDownloader::retrieveUrl: no content for " << url << endl;
#endif
		}
		else
		{
			cerr << "Couldn't download " << url << ": " << curl_easy_strerror(res) << endl;
		}

		if (pContentInfo->m_pContent != NULL)
		{
			free(pContentInfo->m_pContent);
		}
		delete pContentInfo;

		// Cleanup
		curl_easy_cleanup(pCurlHandler);
	}

	return pDocument;
}
