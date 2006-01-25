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

#ifdef HAS_GOOGLEAPI
#include "GoogleAPIEngine.h"
#endif
#ifdef HAS_OSAPI
#include "ObjectsSearchAPIEngine.h"
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

/// Returns a SearchEngine of the specified type; NULL if unavailable.
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
#ifdef HAS_GOOGLEAPI
	else if (type == "googleapi")
	{
		myEngine = new GoogleAPIEngine();
		myEngine->setKey(option);
	}
#endif
#ifdef HAS_OSAPI
	else if (type == "objectssearchapi")
	{
		myEngine = new ObjectsSearchAPIEngine();
	}
#endif

	return myEngine;
}

/// Indicates whether a search engine is supported or not.
bool SearchEngineFactory::isSupported(const string &type)
{
	if (
#ifdef HAS_GOOGLEAPI
		(type == "googleapi") ||
#endif
#ifdef HAS_OSAPI
		(type == "objectssearchapi") ||
#endif
		(type == "sherlock") ||
		(type == "xapian"))
	{
		return true;
	}

	return false;	
}
