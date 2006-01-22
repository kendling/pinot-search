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

#ifndef _SHERLOCK_PARSER_H
#define _SHERLOCK_PARSER_H

#include <string>
#include <map>

#include "Document.h"
#include "SearchPluginProperties.h"

/**
  * A parser for Sherlock plugin files.
  * See http://developer.apple.com/technotes/tn/tn1141.html
  * and http://mycroft.mozdev.org/deepdocs/deepdocs.html
  */
class SherlockParser
{
	public:
		SherlockParser(const Document *pDocument);
		virtual ~SherlockParser();

		/// Parses the plugin; false if not all could be parsed.
		bool parse(bool extractSearchParams = false);

		/// Returns the plugin's properties.
		virtual const SearchPluginProperties &getProperties(void);

	protected:
		const Document *m_pDocument;
		SearchPluginProperties m_properties;

	private:
		SherlockParser(const SherlockParser &other);
		SherlockParser& operator=(const SherlockParser& other);

};

#endif // _SHERLOCK_PARSER_H
