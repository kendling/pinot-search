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

#ifndef _DOWNLOADER_INTERFACE_H
#define _DOWNLOADER_INTERFACE_H

#include <string>

#include "Document.h"

/// Interface implemented by downloaders.
class DownloaderInterface
{
	public:
		virtual ~DownloaderInterface();

		/// Initializes downloaders.
		static void initialize(void);

		/// Shutdowns downloaders.
		static void shutdown(void);

		/// Sets a (name, value) setting; true if success.
		virtual bool setSetting(const std::string &name, const std::string &value);

		/// Sets timeout.
		virtual void setTimeout(unsigned int milliseconds);

		/// Retrieves the specified document; NULL if error. Caller deletes.
		virtual Document *retrieveUrl(const DocumentInfo &docInfo) = 0;

	protected:
		unsigned int m_timeout;

		DownloaderInterface();

};

#endif // _DOWNLOADER_INTERFACE_H
