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

#include <strings.h>
#include <glib.h>
#include <iostream>

#include "xdgmime/xdgmime.h"

#include "MIMEScanner.h"
#include "StringManip.h"
#include "Url.h"

// FIXME: "Default Applications" here ?
#define MIME_DEFAULTS_SECTION "MIME Cache"

using std::cout;
using std::endl;
using std::string;
using std::multimap;
using std::vector;
using std::pair;

static string getKeyValue(GKeyFile *pDesktopFile, const string &key)
{
	string value;

	if ((pDesktopFile == NULL) ||
		(key.empty() == true))
	{
		return "";
	}

	GError *pError = NULL;
	gchar *pKeyValue = g_key_file_get_string(pDesktopFile,
		"Desktop Entry", key.c_str(), &pError);

	if (pError == NULL)
	{
		if (pKeyValue != NULL)
		{
			value = pKeyValue;

			g_free(pKeyValue);
		}
	}
	else
	{
		g_error_free(pError);
	}

	return value;
}

MIMEAction::MIMEAction() :
	m_multipleArgs(false),
	m_localOnly(true),
	m_priority(0)
{
}

MIMEAction::MIMEAction(const string &name, const string &cmdLine) :
	m_multipleArgs(false),
	m_localOnly(true),
	m_name(name),
	m_exec(cmdLine),
	m_priority(0)
{
	parseExec();
}

MIMEAction::MIMEAction(const string &desktopFile) :
	m_multipleArgs(false),
	m_localOnly(true),
	m_location(desktopFile),
	m_priority(0)
{
	GKeyFile *pDesktopFile = g_key_file_new();
	GError *pError = NULL;

	if (pDesktopFile != NULL)
	{
		g_key_file_load_from_file(pDesktopFile, (const gchar *)desktopFile.c_str(),
			G_KEY_FILE_NONE, &pError);
		if (pError == NULL)
		{
			m_name = getKeyValue(pDesktopFile, "Name");
			// This may contain parameters described here :
			// http://standards.freedesktop.org/desktop-entry-spec/latest/ar01s06.html
			m_exec = getKeyValue(pDesktopFile, "Exec");
			m_icon = getKeyValue(pDesktopFile, "Icon");
			m_device = getKeyValue(pDesktopFile, "Device");

			parseExec();
		}
		else
		{
			g_error_free(pError);
		}

		g_key_file_free(pDesktopFile);
	}
}

MIMEAction::MIMEAction(const MIMEAction &other) :
	m_multipleArgs(other.m_multipleArgs),
	m_localOnly(other.m_localOnly),
	m_name(other.m_name),
	m_location(other.m_location),
	m_exec(other.m_exec),
	m_icon(other.m_icon),
	m_device(other.m_device),
	m_priority(other.m_priority)
{
}

MIMEAction::~MIMEAction()
{
}

bool MIMEAction::operator<(const MIMEAction &other) const
{
	if (m_priority < other.m_priority)
	{
		return true;
	}
	else if (m_priority == other.m_priority)
	{
		if (m_name < other.m_name)
		{
			return true;
		}
	}

	return false;
}

MIMEAction &MIMEAction::operator=(const MIMEAction &other)
{
	if (this != &other)
	{
		m_multipleArgs = other.m_multipleArgs;
		m_localOnly = other.m_localOnly;
		m_name = other.m_name;
		m_location = other.m_location;
		m_exec = other.m_exec;
		m_icon = other.m_icon;
		m_device = other.m_device;
	}

	return *this;
}

void MIMEAction::parseExec(void)
{
	// Does it support multiple arguments ?
	if ((m_exec.find("%U") != string::npos) ||
		(m_exec.find("%F") != string::npos) ||
		(m_exec.find("%D") != string::npos) ||
		(m_exec.find("%N") != string::npos))
	{
		// Yes, it does
		m_multipleArgs = true;
	}
	// What about URLs as parameters ?
	if ((m_exec.find("%u") != string::npos) ||
		(m_exec.find("%U") != string::npos))
	{
		// It does, therefore it's not exclusively local
		m_localOnly = false;
	}
}

multimap<string, MIMEAction> MIMEScanner::m_defaultActions;

pthread_mutex_t MIMEScanner::m_mutex = PTHREAD_MUTEX_INITIALIZER;

MIMEScanner::MIMEScanner()
{
}

MIMEScanner::~MIMEScanner()
{
}

bool MIMEScanner::initialize(const string &desktopFilesDirectory, const string &mimeInfoCache,
	unsigned int priority)
{
	GError *pError = NULL;
	bool foundActions = false;

	if ((desktopFilesDirectory.empty() == true) ||
		(mimeInfoCache.empty() == true))
	{
		return false;
	}

	GKeyFile *pDefaults = g_key_file_new();
	if (pDefaults == NULL)
	{
		return false;
	}

	g_key_file_load_from_file(pDefaults, (const gchar *)mimeInfoCache.c_str(),
		G_KEY_FILE_NONE, &pError);
	if (pError == NULL)
	{
		gchar **pMimeTypes = g_key_file_get_keys(pDefaults, MIME_DEFAULTS_SECTION,
			NULL, &pError);

		if (pMimeTypes != NULL)
		{
			if (pError == NULL)
			{
				for (unsigned int i = 0; pMimeTypes[i] != NULL; ++i)
				{
					gchar **pDesktopFiles = g_key_file_get_string_list(pDefaults,
						MIME_DEFAULTS_SECTION, pMimeTypes[i], NULL, &pError);

					if (pDesktopFiles == NULL)
					{
						continue;
					}
					if (pError != NULL)
					{
						g_error_free(pError);
						continue;
					}

					for (unsigned int j = 0; pDesktopFiles[j] != NULL; ++j)
					{
						MIMEAction typeAction(desktopFilesDirectory + string(pDesktopFiles[j]));

						// Register all applications for that type
						if (typeAction.m_exec.empty() == false)
						{
							typeAction.m_priority = priority;
							m_defaultActions.insert(pair<string, MIMEAction>(pMimeTypes[i], typeAction));
							foundActions = true;
						}
					}

					g_strfreev(pDesktopFiles);
				}
			}
			else
			{
				g_error_free(pError);
			}

			g_strfreev(pMimeTypes);
		}
	}
	else
	{
		g_error_free(pError);
	}
	g_key_file_free(pDefaults);

	return foundActions;
}

void MIMEScanner::shutdown(void)
{
	xdg_mime_shutdown();
}

string MIMEScanner::scanFileType(const string &fileName)
{
	const char *pType = NULL;

	if (fileName.empty() == true)
	{
		return "";
	}

	// Does it have an obvious extension ?
	if (pthread_mutex_lock(&m_mutex) == 0)
	{
		pType = xdg_mime_get_mime_type_from_file_name(fileName.c_str());

		pthread_mutex_unlock(&m_mutex);
	}

	if ((pType == NULL) ||
		(strncasecmp(pType, xdg_mime_type_unknown, strlen(pType)) == 0))
	{
		return "";
	}

	return pType;
}

/// Finds out the given file's MIME type.
string MIMEScanner::scanFile(const string &fileName)
{
	if (fileName.empty() == true)
	{
		return "";
	}

	string mimeType(scanFileType(fileName));

	if (mimeType.empty() == true)
	{
		const char *pType = NULL;

		// Have a peek at the file
		if (pthread_mutex_lock(&m_mutex) == 0)
		{
			pType = xdg_mime_get_mime_type_for_file(fileName.c_str(), NULL);

			pthread_mutex_unlock(&m_mutex);
		}

		if (pType != NULL)
		{
			mimeType = pType;
		}
		else
		{
			if (xdg_mime_type_unknown != NULL)
			{
				mimeType = xdg_mime_type_unknown;
			}
		}
	}
#ifdef DEBUG
	cout << "MIMEScanner::scanFile: " << fileName << " " << mimeType << endl;
#endif

	return mimeType;
}

/// Finds out the given URL's MIME type.
string MIMEScanner::scanUrl(const Url &urlObj)
{
	string mimeType(scanFileType(urlObj.getFile()));

	if (mimeType.empty() == true)
	{
		// Is it a local file ?
		if (urlObj.getProtocol() == "file")
		{
			string fileName = urlObj.getLocation();
			fileName += "/";
			fileName += urlObj.getFile();

			mimeType = scanFile(fileName);
		}
	}

	if (mimeType.empty() == true)
	{
		if (urlObj.getProtocol() == "http")
		{
			mimeType = "text/html";
		}
		else if (xdg_mime_type_unknown != NULL)
		{
			mimeType = xdg_mime_type_unknown;
		}
	}

	return mimeType;
}

/// Adds a user-defined action for the given type.
void MIMEScanner::addDefaultAction(const std::string &mimeType, const MIMEAction &typeAction)
{
	m_defaultActions.insert(pair<string, MIMEAction>(mimeType, typeAction));
}

/// Determines the default action(s) for the given type.
bool MIMEScanner::getDefaultActions(const string &mimeType, vector<MIMEAction> &typeActions)
{
	multimap<string, MIMEAction>::const_iterator actionIter = m_defaultActions.lower_bound(mimeType);
	multimap<string, MIMEAction>::const_iterator endActionIter = m_defaultActions.upper_bound(mimeType);
	bool foundAction = false;

	typeActions.clear();
	if (actionIter == m_defaultActions.end())
	{
		// Is there an action for any of this type's parents ?
		char **pParentTypes = xdg_mime_list_mime_parents(mimeType.c_str());
		if ((pParentTypes != NULL) &&
			(pParentTypes[0] != NULL))
		{
			for (unsigned int i = 0; pParentTypes[i] != NULL; ++i)
			{
				actionIter = m_defaultActions.lower_bound(pParentTypes[i]);
				endActionIter = m_defaultActions.upper_bound(pParentTypes[i]);
				if (actionIter != m_defaultActions.end())
				{
#ifdef DEBUG
					cout << "MIMEScanner::getDefaultActions: " << mimeType << " has parent type " << pParentTypes[i] << endl;
#endif
					typeActions.reserve(m_defaultActions.count(pParentTypes[i]));
					break;
				}
			}

			free(pParentTypes);
		}
#ifdef DEBUG
		else cout << "MIMEScanner::getDefaultActions: " << mimeType << " has no parent types" << endl;
#endif
	}
	else
	{
		typeActions.reserve(m_defaultActions.count(mimeType));
	}

	// Found anything ?
	for (; actionIter != endActionIter; ++actionIter)
	{
#ifdef DEBUG
		cout << "MIMEScanner::getDefaultActions: action " << actionIter->second.m_name
			<< ", " << actionIter->second.m_priority << endl;
#endif
		typeActions.push_back(actionIter->second);
		foundAction = true;
	}

	return foundAction;
}
