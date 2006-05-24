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

#include <cstdio>
#include <strings.h>
#include <stdarg.h>
#include <iostream>

#include <curl/curl.h>

#include "HtmlTokenizer.h"
#include "HtmlDocument.h"
#include "Url.h"
#include "CurlDownloader.h"

using namespace std;

struct ContentInfo
{
	char *m_pContent;
	size_t m_contentLen;
};

size_t writeCallback(void *pData, size_t dataSize, size_t elementsCount, void *pStream)
{
	ContentInfo *pInfo = NULL;
	size_t totalSize = elementsCount * dataSize;

	if (pStream == NULL)
	{
#ifdef DEBUG
		cout << "writeCallback: no stream" << endl;
#endif
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
#ifdef DEBUG
	cout << "writeCallback: copied " << totalSize << " bytes into buffer" << endl;
#endif

	return totalSize;
}

unsigned int CurlDownloader::m_initialized = 0;

CurlDownloader::CurlDownloader() :
	DownloaderInterface()
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

/// Sets a (name, value) setting; true if success.
bool CurlDownloader::setSetting(const string &name, const string &value)
{
        bool goodSetting = true;

        if (name == "User-Agent")
        {
                m_userAgent = value;
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
#ifdef DEBUG
		cout << "CurlDownloader::retrieveUrl: URL is " << url << endl;
#endif
		curl_easy_setopt(pCurlHandler, CURLOPT_URL, url.c_str());

		CURLcode res = curl_easy_perform(pCurlHandler);
		if (res == CURLE_OK)
		{
			if ((pContentInfo->m_pContent != NULL) &&
				(pContentInfo->m_contentLen > 0))
			{
				char *pContentType = NULL;

				// What's the Content-Type ?
				res = curl_easy_getinfo(pCurlHandler, CURLINFO_CONTENT_TYPE, &pContentType);
				if ((res == CURLE_OK) &&
					(pContentType != NULL))
				{
					if (strstr(pContentType, "html") != NULL)
					{
						pDocument = new HtmlDocument(docInfo);
					}
					else
					{
						pDocument = new Document(docInfo);
					}

					// ...and copy the content into it
					pDocument->setData(pContentInfo->m_pContent, pContentInfo->m_contentLen);
					pDocument->setLocation(url);
					pDocument->setType(pContentType);

#ifdef DEBUG
					cout << "CurlDownloader::retrieveUrl: document size is " << pContentInfo->m_contentLen << endl;
#endif
				}
			}
		}

		delete pContentInfo;

		// Cleanup
		curl_easy_cleanup(pCurlHandler);
	}

	return pDocument;
}
