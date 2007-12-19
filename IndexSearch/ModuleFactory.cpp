/*
 *  Copyright 2007 Fabrice Colin
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#include <iostream>

#include "DBusIndex.h"
#ifdef HAVE_GOOGLEAPI
#include "GoogleAPIEngine.h"
#endif
#include "PluginWebEngine.h"
#include "ModuleFactory.h"
#if 0
#include "XapianDatabaseFactory.h"
#include "XapianIndex.h"
#include "XapianEngine.h"
#endif

#ifdef __CYGWIN__
#define DLOPEN_FLAGS RTLD_NOW
#else
#define DLOPEN_FLAGS (RTLD_NOW|RTLD_LOCAL)
#endif

#define GETMODULETYPEFUNC	"getModuleType"
#define OPENORCREATEINDEXFUNC	"openOrCreateIndex"
#define MERGEINDEXESFUNC	"mergeIndexes"
#define GETINDEXFUNC		"getIndex"
#define GETSEARCHENGINEFUNC	"getSearchEngine"
#define CLOSEALLFUNC		"closeAll"

typedef string (getModuleTypeFunc)(void);
typedef bool (openOrCreateIndexFunc)(const string &, bool &, bool, bool);
typedef bool (mergeIndexesFunc)(const string &, const string &, const string &);
typedef IndexInterface *(getIndexFunc)(const string &);
typedef SearchEngineInterface *(getSearchEngineFunc)(const string &);
typedef void (closeAllFunc)(void);

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::map;
using std::set;

map<string, string> ModuleFactory::m_types;
map<string, void *> ModuleFactory::m_handles;

ModuleFactory::ModuleFactory()
{
}

ModuleFactory::~ModuleFactory()
{
}

IndexInterface *ModuleFactory::getLibraryIndex(const string &type, const string &option)
{
	void *pHandle = NULL;

	map<string, string>::iterator typeIter = m_types.find(type);
	if (typeIter == m_types.end())
	{
		// We don't know about this type
		return NULL;
	}
	map<string, void *>::iterator handleIter = m_handles.find(typeIter->second);
	if (handleIter == m_handles.end())
	{
		// We don't know about this library
		return NULL;
	}
	pHandle = handleIter->second;
	if (pHandle == NULL)
	{
		return NULL;
	}

	getIndexFunc *pFunc = (getIndexFunc *)dlsym(pHandle,
		GETINDEXFUNC);
	if (pFunc != NULL)
	{
		return (*pFunc)(option);
	}
#ifdef DEBUG
	cout << "ModuleFactory::getLibraryIndex: couldn't find export getIndex" << endl;
#endif

	return NULL;
}

SearchEngineInterface *ModuleFactory::getLibrarySearchEngine(const string &type, const string &option)
{
	void *pHandle = NULL;

	map<string, string>::iterator typeIter = m_types.find(type);
	if (typeIter == m_types.end())
	{
		// We don't know about this type
		return NULL;
	}
	map<string, void *>::iterator handleIter = m_handles.find(typeIter->second);
	if (handleIter == m_handles.end())
	{
		// We don't know about this library
		return NULL;
	}
	pHandle = handleIter->second;
	if (pHandle == NULL)
	{
		return NULL;
	}

	getSearchEngineFunc *pFunc = (getSearchEngineFunc *)dlsym(pHandle,
		GETSEARCHENGINEFUNC);
	if (pFunc != NULL)
	{
		return (*pFunc)(option);
	}
#ifdef DEBUG
	cout << "ModuleFactory::getLibrarySearchEngine: couldn't find export getSearchEngine" << endl;
#endif

	return NULL;
}

unsigned int ModuleFactory::loadModules(const string &directory)
{
	struct stat fileStat;
	unsigned int count = 0;

	if (directory.empty() == true)
	{
		return 0;
	}

	// Is it a directory ?
	if ((stat(directory.c_str(), &fileStat) == -1) ||
		(!S_ISDIR(fileStat.st_mode)))
	{
		cerr << "ModuleFactory::loadModules: " << directory << " is not a directory" << endl;
		return 0;
	}

	// Scan it
	DIR *pDir = opendir(directory.c_str());
	if (pDir == NULL)
	{
		return 0;
	}

	// Iterate through this directory's entries
	struct dirent *pDirEntry = readdir(pDir);
	while (pDirEntry != NULL)
	{
		char *pEntryName = pDirEntry->d_name;
		if (pEntryName != NULL)
		{
			string fileName = pEntryName;
			string::size_type extPos = fileName.find_last_of(".");

			if ((extPos == string::npos) ||
				(fileName.substr(extPos) != ".so"))
			{
				// Next entry
				pDirEntry = readdir(pDir);
				continue;
			}

			fileName = directory;
			fileName += "/";
			fileName += pEntryName;

			// Check this entry
			if ((stat(fileName.c_str(), &fileStat) == 0) &&
				(S_ISREG(fileStat.st_mode)))
			{
				void *pHandle = dlopen(fileName.c_str(), DLOPEN_FLAGS);
				if (pHandle != NULL)
				{
					// What type does this export ?
					getModuleTypeFunc *pTypeFunc = (getModuleTypeFunc *)dlsym(pHandle,
						GETMODULETYPEFUNC);
					if (pTypeFunc != NULL)
					{
						string moduleType((*pTypeFunc)());

						// Add a record for this module
						m_types[moduleType] = fileName;
#ifdef DEBUG
						cout << "ModuleFactory::loadModules: type " << moduleType
							<< " is supported by " << pEntryName << endl;
#endif
						m_handles[fileName] = pHandle;
					}
					else cerr << "ModuleFactory::loadModules: " << dlerror() << endl;
				}
				else cerr << "ModuleFactory::loadModules: " << dlerror() << endl;
			}
#ifdef DEBUG
			else cout << "ModuleFactory::loadModules: "
				<< pEntryName << " is not a file" << endl;
#endif
		}

		// Next entry
		pDirEntry = readdir(pDir);
	}
	closedir(pDir);

	return count;
}

bool ModuleFactory::openOrCreateIndex(const string &type, const string &option,
	bool &obsoleteFormat, bool readOnly, bool overwrite)
{
	void *pHandle = NULL;

	map<string, string>::iterator typeIter = m_types.find(type);
	if (typeIter == m_types.end())
	{
		// We don't know about this type
		return false;
	}
	map<string, void *>::iterator handleIter = m_handles.find(typeIter->second);
	if (handleIter == m_handles.end())
	{
		// We don't know about this library
		return false;
	}
	pHandle = handleIter->second;
	if (pHandle == NULL)
	{
		return false;
	}

	openOrCreateIndexFunc *pFunc = (openOrCreateIndexFunc *)dlsym(pHandle,
		OPENORCREATEINDEXFUNC);
	if (pFunc != NULL)
	{
		return (*pFunc)(option, obsoleteFormat, readOnly, overwrite);
	}
#ifdef DEBUG
	cout << "ModuleFactory::openOrCreateIndex: couldn't find export openOrCreateIndex" << endl;
#endif

	return false;
}

bool ModuleFactory::mergeIndexes(const string &type, const string &option0,
	const string &option1, const string &option2)
{
	void *pHandle = NULL;

	map<string, string>::iterator typeIter = m_types.find(type);
	if (typeIter == m_types.end())
	{
		// We don't know about this type
		return false;
	}
	map<string, void *>::iterator handleIter = m_handles.find(typeIter->second);
	if (handleIter == m_handles.end())
	{
		// We don't know about this library
		return false;
	}
	pHandle = handleIter->second;
	if (pHandle == NULL)
	{
		return false;
	}

	mergeIndexesFunc *pFunc = (mergeIndexesFunc *)dlsym(pHandle,
		MERGEINDEXESFUNC);
	if (pFunc != NULL)
	{
		return (*pFunc)(option0, option1, option2);
	}
#ifdef DEBUG
	cout << "ModuleFactory::mergeIndexes: couldn't find export mergeIndexes" << endl;
#endif

	return false;
}

IndexInterface *ModuleFactory::getIndex(const string &type, const string &option)
{
	IndexInterface *pIndex = NULL;

	// Choice by type
	// Do we need to nest it in a DBusIndex ?
	if (type.substr(0, 5) == "dbus-")
	{
#ifdef DEBUG
		cout << "ModuleFactory::mergeIndexes: sub-type " << type.substr(5) << endl;
#endif
		pIndex = getLibraryIndex(type.substr(5), option);
		if (pIndex != NULL)
		{
			return new DBusIndex(pIndex);
		}

		return NULL;
	}

	return getLibraryIndex(type, option);
}

SearchEngineInterface *ModuleFactory::getSearchEngine(const string &type, const string &option)
{
	SearchEngineInterface *pEngine = NULL;

	// Choice by type
	if ((type == "sherlock") ||
		(type == "opensearch"))
	{
		pEngine = new PluginWebEngine(option);
	}
#ifdef HAVE_GOOGLEAPI
	else if (type == "googleapi")
	{
		pEngine = new GoogleAPIEngine(option);
	}
#endif

	if (pEngine != NULL)
	{
		return pEngine;
	}

	return getLibrarySearchEngine(type, option);
}

string ModuleFactory::getSearchEngineName(const string &type, const string &option)
{
	if ((type == "sherlock") ||
		(type == "opensearch"))
	{
		string name, channel;

		if (PluginWebEngine::getDetails(option, name, channel) == true)
		{
			return name;
		}

		return "";
	}
	else
	{
		return option;
	}

	return type;
}

void ModuleFactory::getSupportedEngines(set<string> &engines)
{
	engines.clear();

	// Built-in engines
	engines.insert("sherlock");
	engines.insert("opensearch");
#ifdef HAVE_GOOGLEAPI
	engines.insert("googleapi");
#endif
	// Library-handled engines
	for (map<string, string>::iterator typeIter = m_types.begin();
		typeIter != m_types.end(); ++typeIter)
	{
		engines.insert(typeIter->first);
	}
}

bool ModuleFactory::isSupported(const string &type)
{
	if (
#ifdef HAVE_GOOGLEAPI
		(type == "googleapi") ||
#endif
		(type == "sherlock") ||
		(m_types.find(type) != m_types.end()))
	{
		return true;
	}

	return false;	
}

void ModuleFactory::unloadModules(void)
{
	for (map<string, void*>::iterator iter = m_handles.begin(); iter != m_handles.end(); ++iter)
	{
		void *pHandle = iter->second;

		closeAllFunc *pFunc = (closeAllFunc *)dlsym(pHandle, CLOSEALLFUNC);
		if (pFunc != NULL)
		{
			(*pFunc)();
		}
#ifdef DEBUG
		else cout << "ModuleFactory::unloadModules: couldn't find export closeAll" << endl;
#endif

		if (dlclose(pHandle) != 0)
		{
#ifdef DEBUG
			cout << "ModuleFactory::unloadModules: failed on " << iter->first << endl;
#endif
		}
	}

	m_types.clear();
	m_handles.clear();
}

