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

#ifndef _HTML_TOKENIZER_H
#define _HTML_TOKENIZER_H

#include <string>
#include <map>
#include <utility>
#include <set>

#include "Document.h"
#include "Tokenizer.h"

/// A link in an HTML page.
class Link
{
	public:
		Link();
		Link(const Link &other);
		~Link();

		Link& operator=(const Link& other);
		bool operator==(const Link &other) const;
		bool operator<(const Link &other) const;

		std::string m_url;
		std::string m_name;
		unsigned int m_index;
		unsigned int m_startPos;
		unsigned int m_endPos;

};

/// Tokenizer for HTML.
class HtmlTokenizer : public Tokenizer
{
	public:
		HtmlTokenizer(const Document *pDocument,
			bool validateOnly, bool findAbstract = false);
		virtual ~HtmlTokenizer();

		/// Initializes the HTML tokenizer.
		static void initialize(void);

		/// Shutdowns the HTML tokenizer.
		static void shutdown(void);

		/// Determines whether the document is properly formed.
		bool isDocumentValid(void) const;

		/// Gets the specified META tag content; an empty string if it wasn't found.
		std::string getMetaTag(const std::string &name) const;

		/// Gets the links map.
		std::set<Link> &getLinks(void);

		/// Gets the abstract.
		std::string getAbstract(void) const;

		class ParserState
		{
			public:
				ParserState();
				~ParserState();

				bool m_isValid;
				bool m_findAbstract;
				unsigned int m_textPos;
				std::string m_lastHash;
				bool m_inHead;
				bool m_foundHead;
				bool m_appendToTitle;
				bool m_appendToText;
				bool m_appendToLink;
				unsigned int m_skip;
				std::string m_title;
				std::string m_text;
				std::string m_abstract;
				Link m_currentLink;
				std::set<Link> m_links;
				std::set<Link> m_frames;
				std::map<std::string, std::string> m_metaTags;
		};

	protected:
		ParserState m_state;
		Document *m_pStrippedDocument;

		bool parseHTML(const Document *pDocument);

};

#endif // _HTML_TOKENIZER_H
