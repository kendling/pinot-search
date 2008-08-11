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

#ifndef _MIME_SCANNER_H
#define _MIME_SCANNER_H

#include <pthread.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>

#include "Url.h"
#include "Visibility.h"

/** MIMEAction stores useful information extracted from desktop files.
  * The desktop files format is defined by
  * http://standards.freedesktop.org/desktop-entry-spec/latest/
  */
class PINOT_EXPORT MIMEAction
{
	public:
		MIMEAction();
		MIMEAction(const std::string &name, const std::string &cmdLine);
		MIMEAction(const std::string &desktopFile);
		MIMEAction(const MIMEAction &other);
		virtual ~MIMEAction();

		bool operator<(const MIMEAction &other) const;

		MIMEAction &operator=(const MIMEAction &other);

		void load(void);
		void parseExec(void);

		bool m_multipleArgs;
		bool m_localOnly;
		std::string m_name;
		std::string m_location;
		std::string m_exec;
		std::string m_icon;
		std::string m_device;

};

/** Caching of MIME type information.
  * A cache can be reloaded as required.
  */
class MIMECache
{
	public:
		MIMECache();
		MIMECache(const std::string &file, const std::string &section);
		MIMECache(const MIMECache& other);
		~MIMECache();

		bool operator<(const MIMECache &other) const;

		MIMECache &operator=(const MIMECache &other);

		bool load(const std::list<std::string> &desktopFilesPaths);
		void reload(const std::list<std::string> &desktopFilesPaths);
  
		std::string m_file;
		std::string m_section;
		std::multimap<std::string, MIMEAction> m_defaultActions;

	protected:
		bool findDesktopFile(const std::string &desktopFile, const std::string &mimeType,
			std::map<std::string, MIMEAction> &loadedActions);

};

/**
  * Utility class to get a file's MIME type and the default application associated with it.
  */
class PINOT_EXPORT MIMEScanner
{
	public:
		~MIMEScanner();

		/// Initializes the MIME system.
		static bool initialize(const std::string &userPrefix, const std::string &systemPrefix);
  
		/// Shutdowns the MIME system.
		static void shutdown(void);

		/// Lists MIME configuration files under the given prefix.
		static void listConfigurationFiles(const std::string &prefix, std::set<std::string> &files);

		/// Finds out the given file's MIME type.
		static std::string scanFile(const std::string &fileName);

		/// Finds out the given URL's MIME type.
		static std::string scanUrl(const Url &urlObj);

		/// Gets parent MIME types.
		static bool getParentTypes(const std::string &mimeType, std::set<std::string> &parentMimeTypes);

		/// Adds a user-defined action for the given type.
		static void addDefaultAction(const std::string &mimeType, const MIMEAction &typeAction);

		/// Determines the default action(s) for the given type.
		static bool getDefaultActions(const std::string &mimeType, std::vector<MIMEAction> &typeActions);

	protected:
#ifndef HAVE_GIO_MIME
		/// Mutex to protect access to xdgmime.
		static pthread_mutex_t m_xdgMutex;
#endif
		/// Lock to protect access to caches.
		static pthread_rwlock_t m_cachesLock;
		/// MIME type caches, ordered by decreasing priority.
		static std::list<MIMECache> m_caches;

		MIMEScanner();

		static std::string scanFileType(const std::string &fileName);
  
		static bool addCache(const std::string &file, const std::string &section,
			const std::list<std::string> &desktopFilesPaths);
  
		static bool getDefaultActionsForType(const std::string &mimeType, std::set<std::string> &actionNames,
			std::vector<MIMEAction> &typeActions);

	private:
		MIMEScanner(const MIMEScanner &other);
		MIMEScanner& operator=(const MIMEScanner& other);

};

#endif // _MIME_SCANNER_H
