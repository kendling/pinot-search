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

#ifndef _CURL_DOWNLOADER_H
#define _CURL_DOWNLOADER_H

#include <string>

#include "DownloaderInterface.h"

class CurlDownloader : public DownloaderInterface
{
	public:
		CurlDownloader();
		virtual ~CurlDownloader();

		/// Sets a (name, value) setting; true if success.
		virtual bool setSetting(const std::string &name, const std::string &value);

		/// Retrieves the specified document; NULL if error. Caller deletes.
		virtual Document *retrieveUrl(const DocumentInfo &docInfo);

	protected:
		static unsigned int m_initialized;
		std::string m_userAgent;

	private:
		CurlDownloader(const CurlDownloader &other);
		CurlDownloader &operator=(const CurlDownloader &other);

};

#endif // _CURL_DOWNLOADER_H
