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

#ifndef _XML_TOKENIZER_H
#define _XML_TOKENIZER_H

#include <string>

#include "Document.h"
#include "Tokenizer.h"

class XmlTokenizer : public Tokenizer
{
	public:
		XmlTokenizer(const Document *pDocument);
		virtual ~XmlTokenizer();

		/// Utility method that strips XML tags off; the string without tags.
		static std::string stripTags(const std::string &str);

	protected:
		const Document *m_pXmlDocument;

		XmlTokenizer();

		void initialize(const Document *pDocument);

		/// Parses XML; the string without tags.
		std::string parseXML(const std::string &str);

};

#endif // _XML_TOKENIZER_H
