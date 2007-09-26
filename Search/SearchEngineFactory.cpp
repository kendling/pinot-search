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

#ifdef HAVE_GOOGLEAPI
#include "GoogleAPIEngine.h"
#endif
#include "PluginWebEngine.h"
#include "XapianEngine.h"
#include "SearchEngineFactory.h"

SearchEngineFactory::SearchEngineFactory()
{
}

SearchEngineFactory::~SearchEngineFactory()
{
}

SearchEngineInterface *SearchEngineFactory::getSearchEngine(const string &type, const string &option)
{
	SearchEngineInterface *myEngine = NULL;

	// Choice by type
	if ((type == "sherlock") ||
		(type == "opensearch"))
	{
		myEngine = new PluginWebEngine(option);
	}
	else if (type == "xapian")
	{
		myEngine = new XapianEngine(option);
	}
#ifdef HAVE_GOOGLEAPI
	else if (type == "googleapi")
	{
		myEngine = new GoogleAPIEngine(option);
	}
#endif

	return myEngine;
}

string SearchEngineFactory::getSearchEngineName(const string &type, const string &option)
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
	else if (type == "xapian")
	{
		return option;
	}

	return type;
}

void SearchEngineFactory::getSupportedEngines(set<string> &engines)
{
	engines.clear();

	// List supported engines
	engines.insert("sherlock");
	engines.insert("opensearch");
	engines.insert("xapian");
#ifdef HAVE_GOOGLEAPI
	engines.insert("googleapi");
#endif
}

bool SearchEngineFactory::isSupported(const string &type)
{
	if (
#ifdef HAVE_GOOGLEAPI
		(type == "googleapi") ||
#endif
		(type == "sherlock") ||
		(type == "xapian"))
	{
		return true;
	}

	return false;	
}
