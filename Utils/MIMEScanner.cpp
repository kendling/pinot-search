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

#include <strings.h>
#include <glib.h>
#include <iostream>

#include "config.h"
#include "xdgmime/xdgmime.h"

#include "MIMEScanner.h"
#include "StringManip.h"
#include "Url.h"

// FIXME: or should it be defaults.list ?
#define MIME_DEFAULTS_FILE "mimeinfo.cache"
// FIXME: "Default Applications" here ?
#define MIME_DEFAULTS_SECTION "MIME Cache"

using std::cout;
using std::endl;
using std::string;
using std::map;

static MIMEAction getTypeAction(const string &desktopFile)
{
	MIMEAction typeAction;
	GKeyFile *pDesktopFile = g_key_file_new();
	GError *pError = NULL;

	if (pDesktopFile == NULL)
	{
		return typeAction;
	}

	g_key_file_load_from_file(pDesktopFile, (const gchar *)desktopFile.c_str(),
		G_KEY_FILE_NONE, &pError);
	if (pError == NULL)
	{
		gchar *pExecKey = g_key_file_get_string(pDesktopFile,
			"Desktop Entry", "Exec", &pError);

		if (pError == NULL)
		{
			if (pExecKey != NULL)
			{
				typeAction.m_exec = pExecKey;

				// Does it support URLs as parameter, as described here ?
				// http://standards.freedesktop.org/desktop-entry-spec/latest/ar01s06.html
				if ((typeAction.m_exec.find("%u") != string::npos) ||
					(typeAction.m_exec.find("%U") != string::npos))
				{
					// It does, therefore it's not exclusively local
					typeAction.m_localOnly = false;
				}

				g_free(pExecKey);
			}
		}
		else
		{
			g_error_free(pError);
		}
	}
	else
	{
		g_error_free(pError);
	}

	g_key_file_free(pDesktopFile);

	return typeAction;
}

MIMEAction::MIMEAction() :
	m_localOnly(true)
{
}

MIMEAction::MIMEAction(const MIMEAction &other) :
	m_localOnly(other.m_localOnly),
	m_exec(other.m_exec)
{
}

MIMEAction::~MIMEAction()
{
}

MIMEAction &MIMEAction::operator=(const MIMEAction &other)
{
	m_localOnly = other.m_localOnly;
	m_exec = other.m_exec;

	return *this;
}

map<string, MIMEAction> MIMEScanner::m_defaultActions;

MIMEScanner::MIMEScanner()
{
}

MIMEScanner::~MIMEScanner()
{
}

void MIMEScanner::initialize(void)
{
	GError *pError = NULL;

	GKeyFile *pDefaults = g_key_file_new();
	if (pDefaults == NULL)
	{
		return;
	}

	string desktopFilesDirectory(PREFIX);
	desktopFilesDirectory += "/share/applications/";

	string defaultsFile(desktopFilesDirectory);
	defaultsFile += MIME_DEFAULTS_FILE;

	g_key_file_load_from_file(pDefaults, (const gchar *)defaultsFile.c_str(),
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
					unsigned int defaultApp = 0;

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
						defaultApp = j;
					}

					// Register the last application
					if (pDesktopFiles[defaultApp] != NULL)
					{
						MIMEAction typeAction(getTypeAction(desktopFilesDirectory + string(pDesktopFiles[defaultApp])));

						if (typeAction.m_exec.empty() == false)
						{
#ifdef DEBUG
							cout << "MIMEScanner::initialize: default for " << pMimeTypes[i]
								<< " is " << typeAction.m_exec << endl;
#endif
							m_defaultActions[pMimeTypes[i]] = typeAction;
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

	// xdg_mime_init() is called automatically when needed
}

void MIMEScanner::shutdown(void)
{
	xdg_mime_shutdown();
}

string MIMEScanner::scanFileType(const string &fileName)
{
	if (fileName.empty() == true)
	{
		return "";
	}

	// Does it have an obvious extension ?
	const char *pType = xdg_mime_get_mime_type_from_file_name(fileName.c_str());
	if ((pType == NULL) ||
		(strncasecmp(pType, xdg_mime_type_unknown, strlen(pType)) == 0))
	{
#ifdef DEBUG
		cout << "MIMEScanner::scanFileType: couldn't determine type of " << fileName << endl;
#endif
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
		// Have a peek at the file
		const char *pType = xdg_mime_get_mime_type_for_file(fileName.c_str(), NULL);
		if ((pType != NULL) &&
			(strncasecmp(pType, xdg_mime_type_unknown, strlen(pType)) != 0))
		{
			return pType;
		}

#ifdef DEBUG
		cout << "MIMEScanner::scanFile: couldn't determine type of " << fileName << endl;
#endif
		if (xdg_mime_type_unknown != NULL)
		{
			mimeType = xdg_mime_type_unknown;
		}
	}

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

/// Determines the default action for the given type.
bool MIMEScanner::getDefaultAction(const string &mimeType, MIMEAction &typeAction)
{
	map<string, MIMEAction>::const_iterator actionIter = m_defaultActions.find(mimeType);

	if (actionIter != m_defaultActions.end())
	{
		typeAction = actionIter->second;

		return true;
	}

	return false;
}
