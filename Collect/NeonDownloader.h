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

#ifndef _NEON_DOWNLOADER_H
#define _NEON_DOWNLOADER_H

#include <string>

#include <neon/ne_socket.h>
#include <neon/ne_session.h>
#include <neon/ne_request.h>

#include "DownloaderInterface.h"

class NeonDownloader : public DownloaderInterface
{
	public:
		NeonDownloader();
		virtual ~NeonDownloader();

		/// Sets a (name, value) setting; true if success.
		virtual bool setSetting(const std::string &name, const std::string &value);

		/// Retrieves the specified document; NULL if error. Caller deletes.
		virtual Document *retrieveUrl(const DocumentInfo &docInfo);

		/// Stops the current action.
		virtual bool stop(void);

	protected:
		static bool m_initialized;
		std::string m_userAgent;
		ne_session *m_pSession;
		ne_request *m_pRequest;

		std::string handleRedirection(const char *pBody, unsigned int length);

	private:
		NeonDownloader(const NeonDownloader &other);
		NeonDownloader &operator=(const NeonDownloader &other);

};

#endif // _NEON_DOWNLOADER_H
