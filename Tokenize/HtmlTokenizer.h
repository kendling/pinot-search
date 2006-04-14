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

#ifndef _HTML_TOKENIZER_H
#define _HTML_TOKENIZER_H

#include <string>
#include <map>
#include <utility>
#include <set>

#include "Document.h"
#include "Tokenizer.h"

class Link
{
	public:
		Link(const std::string &url, const std::string &name,
			unsigned int pos, unsigned int openPos, unsigned int closePos);
		Link(const Link &other);
		~Link();

		Link& operator=(const Link& other);
		bool operator==(const Link &other) const;
		bool operator<(const Link &other) const;

		std::string m_url;
		std::string m_name;
		unsigned int m_pos;
		unsigned int m_open;
		unsigned int m_close;

};

class HtmlTokenizer : public Tokenizer
{
	public:
		HtmlTokenizer(const Document *pDocument, unsigned int linksStartAtPos = 0);
		virtual ~HtmlTokenizer();

		/// Gets the specified META tag content; an empty string if it wasn't found.
		std::string getMetaTag(const std::string &name);

		/// Gets the links map.
		std::set<Link> &getLinks(void);

		/// Utility method that strips HTML tags off; the string without tags.
		static std::string stripTags(const std::string &str);

	protected:
		const Document *m_pHtmlDocument;
		unsigned int m_linkPos;
		std::map<std::string, std::string> m_metaTags;
		std::set<Link> m_links;

		HtmlTokenizer();

		void initialize(const Document *pDocument);

		/// Parses HTML; the string without tags.
		std::string parseHTML(const std::string &str, bool stripAllBlocks = false);

		/// Returns true if the tag corresponds to a text block.
		static bool textBlockStart(const std::string &tag);

		/// Returns true if the tag corresponds to the end of a text block.
		static bool textBlockEnd(const std::string &tag);

};

#endif // _HTML_TOKENIZER_H
