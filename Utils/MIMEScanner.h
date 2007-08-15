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

#ifndef _MIME_SCANNER_H
#define _MIME_SCANNER_H

#include <pthread.h>
#include <string>
#include <map>
#include <vector>

#include "Url.h"

/** MIMEAction stores useful information extracted from desktop files.
  * The desktop files format is defined by
  * http://standards.freedesktop.org/desktop-entry-spec/latest/
  */
class MIMEAction
{
	public:
		MIMEAction();
		MIMEAction(const std::string &name, const std::string &cmdLine);
		MIMEAction(const std::string &desktopFile);
		MIMEAction(const MIMEAction &other);
		virtual ~MIMEAction();

		bool operator<(const MIMEAction &other) const;

		MIMEAction &operator=(const MIMEAction &other);

		void parseExec(void);

		bool m_multipleArgs;
		bool m_localOnly;
		std::string m_name;
		std::string m_location;
		std::string m_exec;
		std::string m_icon;
		std::string m_device;
		unsigned int m_priority;

};

/**
  * Utility class to get a file's MIME type and the default application associated with it.
  */
class MIMEScanner
{
	public:
		~MIMEScanner();

		/// Initializes the MIME system.
		static bool initialize(const std::string &desktopFilesDirectory,
			const std::string &mimeInfoCache, unsigned int priority = 0);

		/// Shutdowns the MIME system.
		static void shutdown(void);

		/// Finds out the given file's MIME type.
		static std::string scanFile(const std::string &fileName);

		/// Finds out the given URL's MIME type.
		static std::string scanUrl(const Url &urlObj);

		/// Adds a user-defined action for the given type.
		static void addDefaultAction(const std::string &mimeType, const MIMEAction &typeAction);

		/// Determines the default action(s) for the given type.
		static bool getDefaultActions(const std::string &mimeType, std::vector<MIMEAction> &typeActions);

	protected:
		static pthread_mutex_t m_mutex;
		static std::multimap<std::string, MIMEAction> m_defaultActions;

		MIMEScanner();

		static std::string scanFileType(const std::string &fileName);

	private:
		MIMEScanner(const MIMEScanner &other);
		MIMEScanner& operator=(const MIMEScanner& other);

};

#endif // _MIME_SCANNER_H
