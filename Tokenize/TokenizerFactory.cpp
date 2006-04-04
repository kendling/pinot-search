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

#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <dlfcn.h>
#include <iostream>

#include "MIMEScanner.h"
#include "Tokenizer.h"
#include "HtmlTokenizer.h"
#include "UnknownTypeTokenizer.h"
#include "Url.h"
#include "TokenizerFactory.h"

#define GETTOKENIZER		"_Z12getTokenizerPK8Document"
#define GETTOKENIZERTYPE	"_Z16getTokenizerTypej"

using std::cout;
using std::endl;

map<string, string> TokenizerFactory::m_types;
map<string, void *> TokenizerFactory::m_handles;

TokenizerFactory::TokenizerFactory()
{
}

TokenizerFactory::~TokenizerFactory()
{
}

Tokenizer *TokenizerFactory::getLibraryTokenizer(const string &type, const Document *pDocument)
{
	void *pHandle = NULL;

	if (m_handles.empty() == true)
	{
#ifdef DEBUG
		cout << "TokenizerFactory::getLibraryTokenizer: no libraries" << endl;
#endif
		return NULL;
	}

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

	// Get a tokenizer object then
	getTokenizerFunc *pFunc = (getTokenizerFunc *)dlsym(pHandle,
		GETTOKENIZER);
	if (pFunc != NULL)
	{
		return (*pFunc)(pDocument);
	}
#ifdef DEBUG
	cout << "TokenizerFactory::getLibraryTokenizer: couldn't find export getTokenizer" << endl;
#endif

	return NULL;
}

/// Loads the tokenizer libraries in the given directory.
unsigned int TokenizerFactory::loadTokenizers(const string &dirName)
{
	struct stat fileStat;
	unsigned int count = 0;

	if (dirName.empty() == true)
	{
		return 0;
	}

	if (stat(dirName.c_str(), &fileStat) == -1)
	{
#ifdef DEBUG
		cout << "TokenizerFactory::loadTokenizers: "
			<< dirName << " doesn't exist" << endl;
#endif
		return 0;
	}

	// Is it a file or a directory ?
	if (S_ISDIR(fileStat.st_mode))
	{
		// A directory : scan it
		DIR *pDir = opendir(dirName.c_str());
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

				fileName = dirName;
				fileName += "/";
				fileName += pEntryName;

				// Check this entry
				if ((stat(fileName.c_str(), &fileStat) == 0) &&
					(S_ISREG(fileStat.st_mode)))
				{
					void *pHandle = dlopen(fileName.c_str(), RTLD_LAZY|RTLD_LOCAL);
					if (pHandle != NULL)
					{
						// What type(s) does this support ?
						getTokenizerTypeFunc *pFunc = (getTokenizerTypeFunc *)dlsym(pHandle,
								GETTOKENIZERTYPE);
						if (pFunc != NULL)
						{
							unsigned int typeNum = 0;

							while (1)
							{
								char *pSupportedType = (*pFunc)(typeNum);
								if (pSupportedType == NULL)
								{
									break;
								}

								// Add a record for this tokenizer
								m_types[pSupportedType] = fileName;
#ifdef DEBUG
								cout << "TokenizerFactory::loadTokenizers: type "
									<< pSupportedType << " supported by " << pEntryName << endl;
#endif
								// It's supposed to have been allocated with new[]
								delete[] pSupportedType;

								// Next
								++count;
								++typeNum;
							}

							if (typeNum > 0)
							{
								m_handles[fileName] = pHandle;
							}
						}
#ifdef DEBUG
						else cout << "TokenizerFactory::loadTokenizers: " << dlerror() << endl;
#endif
					}
#ifdef DEBUG
					else cout << "TokenizerFactory::loadTokenizers: " << dlerror() << endl;
#endif
				}
#ifdef DEBUG
				else cout << "TokenizerFactory::loadTokenizers: "
					<< pEntryName << " is not a file" << endl;
#endif
			}

			// Next entry
			pDirEntry = readdir(pDir);
		}

		closedir(pDir);
	}
#ifdef DEBUG
	else cout << "TokenizerFactory::loadTokenizers: "
		<< dirName << " is not a directory" << endl;
#endif

	return count;
}

/// Unloads all tokenizer libraries.
void TokenizerFactory::unloadTokenizers(void)
{
	for (map<string, void*>::iterator iter = m_handles.begin(); iter != m_handles.end(); ++iter)
	{
		if (dlclose(iter->second) != 0)
		{
#ifdef DEBUG
			cout << "TokenizerFactory::unloadTokenizers: failed on " << iter->first << endl;
#endif
		}
	}

	m_types.clear();
	m_handles.clear();
}

/// Returns a Tokenizer that handles the given file's type; NULL if unavailable.
Tokenizer *TokenizerFactory::getTokenizer(const string &fileName, const Document *pDocument)
{
	string type = MIMEScanner::scanFile(fileName);

	return getTokenizerByType(type, pDocument);
}

/// Returns a Tokenizer that handles the given MIME type; NULL if unavailable.
Tokenizer *TokenizerFactory::getTokenizerByType(const string &type, const Document *pDocument)
{
	string typeOnly = type;
	string::size_type semiColonPos = type.find(";");

	// Remove the charset, if any
	if (semiColonPos != string::npos)
	{
		typeOnly = type.substr(0, semiColonPos);
	}
#ifdef DEBUG
	cout << "TokenizerFactory::getTokenizerByType: file type is " << typeOnly << endl;
#endif

	if (typeOnly == "text/html")
	{
		return new HtmlTokenizer(pDocument);
	}
	else if (typeOnly == "text/plain")
	{
		return new Tokenizer(pDocument);
	}

	Tokenizer *pTokenizer = getLibraryTokenizer(typeOnly, pDocument);
	if (pTokenizer == NULL)
	{
		if (strncasecmp(typeOnly.c_str(), "text", 4) == 0)
		{
			// Use this by default for text documents
			return new Tokenizer(pDocument);
		}

#ifdef DEBUG
		cout << "TokenizerFactory::getTokenizerByType: unknown file type" << endl;
#endif
		return new UnknownTypeTokenizer(pDocument);
	}

	return pTokenizer;
}

void TokenizerFactory::getSupportedTypes(set<string> &types)
{
	types.clear();

	// List supported types
	types.insert("text/plain");
	types.insert("text/html");
	for (map<string, string>::iterator iter = m_types.begin(); iter != m_types.end(); ++iter)
	{
		types.insert(iter->first);
	}
}

bool TokenizerFactory::isSupportedType(const string &type)
{
	string typeOnly = type;
	string::size_type semiColonPos = type.find(";");

	// Remove the charset, if any
	if (semiColonPos != string::npos)
	{
		typeOnly = type.substr(0, semiColonPos);
	}
#ifdef DEBUG
	cout << "TokenizerFactory::isSupportedType: file type is " << typeOnly << endl;
#endif

	// Is it a built-in type ?
	if ((typeOnly == "text/html") ||
		(strncasecmp(typeOnly.c_str(), "text", 4) == 0))
	{
		return true;
	}

	// Is it a type supported by a library ?
	map<string, string>::iterator typeIter = m_types.find(typeOnly);
	if (typeIter != m_types.end())
	{
		return true;
	}

	return false;
}
