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

#ifndef _PLUGIN_PARSER_H
#define _PLUGIN_PARSER_H

#include <string>
#include <map>

#include "Document.h"

using namespace std;

class PluginProperties
{
	public:
		PluginProperties()
		{
		}
		virtual ~PluginProperties()
		{
		}


		map<string, string> m_searchParams;
		map<string, string> m_inputItems;
		string m_userInput;
		string m_nextInput;
		string m_nextFactor;
		string m_nextValue;
		map<string, string> m_interpretParams;

	private:
		PluginProperties(const PluginProperties &other);
		PluginProperties& operator=(const PluginProperties& other);

};

/// A parser for Sherlock plugin files.
class PluginParser
{
	public:
		PluginParser(const Document *pDocument);
		virtual ~PluginParser();

		/// Parses the plugin; false if not all could be parsed.
		bool parse(bool extractSearchParams = false);

		/// Returns the plugin's properties.
		virtual PluginProperties &getProperties(void);

		/// Returns a pointer to the plugin file's document.
		virtual const Document *getDocument(void);

	protected:
		const Document *m_pDocument;
		PluginProperties m_properties;

	private:
		PluginParser(const PluginParser &other);
		PluginParser& operator=(const PluginParser& other);

};

#endif // _PLUGIN_PARSER_H
