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
#include <pthread.h>
#include <iostream>

#include <neon/ne_session.h>
// Does Neon have OpenSSL support ?
#ifdef NE_SSL_H
#include <openssl/crypto.h>
#endif	// NE_SSL_H

#include "HtmlTokenizer.h"
#include "HtmlDocument.h"
#include "Url.h"
#include "NeonDownloader.h"

using namespace std;

static string g_locationHeaderValue;
static string g_contentTypeHeaderValue;
static void headerHandler(void *userdata, const char *value)
{
	long headerNum = (long)userdata;
	if (headerNum == 1)
	{
		// Location
		if (value == NULL)
		{
			g_locationHeaderValue.clear();
		}
		else
		{
			g_locationHeaderValue = value;
		}
	}
	else if (headerNum == 2)
	{
		// Content-Type
		if (value == NULL)
		{
			g_contentTypeHeaderValue.clear();
		}
		else
		{
			g_contentTypeHeaderValue = value;
		}
	}
}

bool NeonDownloader::m_initialized = false;

#ifdef NE_SSL_H
// OpenSSL multi-thread support, required by Neon
static pthread_mutex_t locksTable[CRYPTO_NUM_LOCKS];

// OpenSSL locking functiom
static void lockingCallback(int mode, int n, const char *file, int line)
{
	int status = 0;

	if (mode & CRYPTO_LOCK)
	{
		status = pthread_mutex_lock(&(locksTable[n]));
#ifdef DEBUG
		if (status != 0)
		{
			cout << "lockingCallback: failed to lock mutex " << n << endl;
		}
#endif
	}
	else
	{
		status = pthread_mutex_unlock(&(locksTable[n]));
#ifdef DEBUG
		if (status != 0)
		{
			cout << "lockingCallback: failed to unlock mutex " << n << endl;
		}
#endif
	}
}

unsigned long idCallback(void)
{
	return (unsigned long)pthread_self();
}
#endif // NE_SSL_H

/// Initialize the downloader.
void NeonDownloader::initialize(void)
{
#ifdef NE_SSL_H
	pthread_mutexattr_t mutexAttr;

	pthread_mutexattr_init(&mutexAttr);
	pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_ERRORCHECK);

	// Initialize the OpenSSL mutexes
	for (unsigned int lockNum = 0; lockNum < CRYPTO_NUM_LOCKS; ++lockNum)
	{
		pthread_mutex_init(&(locksTable[lockNum]), &mutexAttr);
	}
	// Set the callbacks
	CRYPTO_set_locking_callback(lockingCallback);
	CRYPTO_set_id_callback(idCallback);
#endif	// NE_SSL_H
}

/// Shutdown the downloader.
void NeonDownloader::shutdown(void)
{
#ifdef NE_SSL_H
	// Reset the OpenSSL callbacks
	CRYPTO_set_id_callback(NULL);
	CRYPTO_set_locking_callback(NULL);
	// Free the mutexes
	for (unsigned int lockNum = 0; lockNum < CRYPTO_NUM_LOCKS; ++lockNum)
	{
		pthread_mutex_destroy(&(locksTable[lockNum]));
	}
#endif	// NE_SSL_H
}

NeonDownloader::NeonDownloader() :
	DownloaderInterface(),
	m_pSession(NULL),
	m_pRequest(NULL)
{
	// Pretend to be Mozilla
	m_userAgent = "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.7.3) Gecko/20041020";
	if (m_initialized == false)
	{
		ne_sock_init();
		m_initialized = true;
	}
}

NeonDownloader::~NeonDownloader()
{
	// Cleanup
	if (m_pRequest != NULL)
	{
		ne_request_destroy(m_pRequest);
	}
	if (m_pSession != NULL)
	{
		ne_session_destroy(m_pSession);
	}
}

string NeonDownloader::handleRedirection(const char *pBody, unsigned int length)
{
	if ((pBody == NULL) ||
		(length == 0))
	{
		return "";
	}

	Document doc;
	doc.setData(pBody, length);
	HtmlTokenizer tokens(&doc);

	// Extract the link from the 3xx message
	set<Link> linksSet = tokens.getLinks();
	// There should be one and only one
	if (linksSet.size() != 1)
	{
#ifdef DEBUG
		cout << "NeonDownloader::handleRedirection: " << linksSet.size() << " links found in " << length << " bytes" << endl;
		cout << "NeonDownloader::handleRedirection: redirection message was " << pBody << endl;
#endif
		return "";
	}
	set<Link>::const_iterator iter = linksSet.begin();
	if (iter != linksSet.end())
	{
		// Update the URL
		return iter->m_url;
	}

	return "";
}

//
// Implementation of DownloaderInterface
//

/// Sets a (name, value) setting; true if success.
bool NeonDownloader::setSetting(const string &name, const string &value)
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
Document *NeonDownloader::retrieveUrl(const DocumentInfo &docInfo)
{
	Document *urlDocument = NULL;
	string url = Url::escapeUrl(docInfo.getLocation());
	char *content = NULL;
	size_t contentLen = 0;
	int statusCode = 200;
	unsigned int redirectionsCount = 0;

	if (url.empty() == true)
	{
#ifdef DEBUG
		cerr << "NeonDownloader::retrieveUrl: no URL specified !" << endl;
#endif
		return NULL;
	}
	Url urlObj(url);
	string protocol = urlObj.getProtocol();
	string hostName = urlObj.getHost();
	string location = urlObj.getLocation();
	string file = urlObj.getFile();
	string parameters = urlObj.getParameters();

	// Create a session
	m_pSession = ne_session_create(protocol.c_str(), hostName.c_str(), 80); // urlObj.getPort());
	if (m_pSession == NULL)
	{
#ifdef DEBUG
		cerr << "NeonDownloader::retrieveUrl: couldn't create session !" << endl;
#endif
		return NULL;
	}
	// Set the user agent
	ne_set_useragent(m_pSession, m_userAgent.c_str());
	// ...and the timeout
	ne_set_read_timeout(m_pSession, (int)m_timeout);

	string fullLocation = "/";
	if (location.empty() == false)
	{
		fullLocation += location;
	}
	if (file.empty() == false)
	{
		if (location.empty() == false)
		{
			fullLocation += "/";
		}
		fullLocation += file;
	}
	if (parameters.empty() == false)
	{
		fullLocation += "?";
		fullLocation += parameters;
	}

	// Create a request for this URL
	m_pRequest = ne_request_create(m_pSession, "GET", fullLocation.c_str());
	if (m_pRequest == NULL)
	{
#ifdef DEBUG
		cerr << "NeonDownloader::retrieveUrl: couldn't create request !" << endl;
#endif
		ne_session_destroy(m_pSession);
		m_pSession = NULL;
		return NULL;
	}
#ifdef DEBUG
	cout << "NeonDownloader::retrieveUrl: request for " << fullLocation << " on " << hostName << endl;
#endif

	int requestStatus = NE_RETRY;
	while (requestStatus == NE_RETRY)
	{
		// FIXME: this is apparently the only way to get the value of an HTTP header
		g_locationHeaderValue.clear();
		ne_add_response_header_handler(m_pRequest, "Location", headerHandler, (void*)1);
		ne_add_response_header_handler(m_pRequest, "Content-Type", headerHandler, (void*)2);

		// Begin the request
		requestStatus = ne_begin_request(m_pRequest);
#ifdef DEBUG
		cout << "NeonDownloader::retrieveUrl: request begun with status " << requestStatus << endl;
#endif
		if (requestStatus == NE_OK)
		{
			ssize_t bytesRead = 0;
			char buffer[1024];

			// Get the status
			const ne_status *pStatus = ne_get_status(m_pRequest);
			if (pStatus != NULL)
			{
				statusCode = pStatus->code;
#ifdef DEBUG
				cout << "NeonDownloader::retrieveUrl: status is " << statusCode << endl;
#endif
			}
			else
			{
				// Assume all is well
				statusCode = 200;
			}

			// Read the content
			while ((bytesRead = ne_read_response_block(m_pRequest, buffer, 1024)) > 0)
			{
				content = (char*)realloc(content, contentLen + bytesRead);
				memcpy((void*)(content + contentLen), (const void*)buffer, bytesRead);
				contentLen += bytesRead;
			}

			// Redirection ?
			if ((statusCode >= 300) &&
				(statusCode < 400) &&
				(redirectionsCount < 10))
			{
				ne_end_request(m_pRequest);
				ne_request_destroy(m_pRequest);
				m_pRequest = NULL;

				string documentUrl = handleRedirection(content, contentLen);
				if (documentUrl.empty() == true)
				{
					// Did we find a Location header ?
					if (g_locationHeaderValue.empty() == true)
					{
						// Fail
						free(content);
						content = NULL;
						contentLen = 0;
						break;
					}
					documentUrl = g_locationHeaderValue;
				}

#ifdef DEBUG
				cout << "NeonDownloader::retrieveUrl: redirected to " << documentUrl << endl;
#endif
				urlObj = Url(documentUrl);
				location = urlObj.getLocation();
				file = urlObj.getFile();

				// Is this on the same host ?
				if (hostName != urlObj.getHost())
				{
					// No, it isn't
					hostName = urlObj.getHost();

					// Create a new session
					ne_session_destroy(m_pSession);
					m_pSession = ne_session_create(protocol.c_str(), hostName.c_str(), 80); // urlObj.getPort());
					if (m_pSession == NULL)
					{
#ifdef DEBUG
						cerr << "NeonDownloader::retrieveUrl: couldn't create session !" << endl;
#endif
						return NULL;
					}
					ne_set_useragent(m_pSession, m_userAgent.c_str());
					ne_set_read_timeout(m_pSession, (int)m_timeout);
				}

				// Try again
				fullLocation = "/";
				if (location.empty() == false)
				{
					fullLocation += location;
					fullLocation += "/";
				}
				if (file.empty() == false)
				{
					fullLocation += file;
				}
#ifdef DEBUG
				cout << "NeonDownloader::retrieveUrl: redirected to " << fullLocation << " on " << hostName << endl;
#endif

				// Create a new request for this URL
				m_pRequest = ne_request_create(m_pSession, "GET", fullLocation.c_str());
				if (m_pRequest == NULL)
				{
#ifdef DEBUG
					cerr << "NeonDownloader::retrieveUrl: couldn't create request !" << endl;
#endif
					ne_session_destroy(m_pSession);
					m_pSession = NULL;
					return NULL;
				}
				redirectionsCount++;
				requestStatus = NE_RETRY;

				// Discard whatever content we have already got
				free(content);
				content = NULL;
				contentLen = 0;
				continue;
			}
		}

		// End the request
		requestStatus = ne_end_request(m_pRequest);
	}

	if ((content != NULL) &&
		(contentLen > 0))
	{
		if (statusCode < 400)
		{
			// Is it an HTML type ?
			if (g_contentTypeHeaderValue.find("html") != string::npos)
			{
				urlDocument = new HtmlDocument(docInfo.getTitle(), url,
					g_contentTypeHeaderValue, docInfo.getLanguage());
			}
			else
			{
				urlDocument = new Document(docInfo.getTitle(), url,
					g_contentTypeHeaderValue, docInfo.getLanguage());
			}
			// ...and copy the content into it
			urlDocument->setData(content, contentLen);
#ifdef DEBUG
			cout << "NeonDownloader::retrieveUrl: document size is " << contentLen << endl;
#endif
		}
		free(content);
	}

	return urlDocument;
}

/// Stops the current action.
bool NeonDownloader::stop(void)
{
	if (m_pRequest != NULL)
	{
		// End the current request
		ne_end_request(m_pRequest);
	}

	return true;
}
