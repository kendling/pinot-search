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

#ifndef _SEARCHENGINE_FACTORY_H
#define _SEARCHENGINE_FACTORY_H

#include <string>
#include <map>
#include <set>

#include "SearchEngineInterface.h"

using std::string;
using std::map;
using std::set;

class SearchEngineFactory
{
	public:
		virtual ~SearchEngineFactory();

		/// Returns a SearchEngine of the specified type; NULL if unavailable.
		static SearchEngineInterface *getSearchEngine(const string &type, const string &option);

		/// Indicates whether a search engine is supported or not.
		static bool isSupported(const string &type);

	protected:
		SearchEngineFactory();

	private:
		SearchEngineFactory(const SearchEngineFactory &other);
		SearchEngineFactory &operator=(const SearchEngineFactory &other);

};

#endif // _SEARCHENGINE_FACTORY_H
