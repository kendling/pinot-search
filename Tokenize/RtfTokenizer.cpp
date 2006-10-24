/*
 *  Copyright 2006,2006 Fabrice Colin
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

#include <string.h>
#include <iostream>

#include "RtfTokenizer.h"

using std::string;
using std::set;

/// This returns the MIME types supported by the library's tokenizer.
bool getTokenizerTypes(set<string> &types)
{
	types.clear();
	types.insert("text/rtf");
	types.insert("application/rtf");

	return true;
}

/// This returns a pointer to a Tokenizer, allocated with new.
Tokenizer *getTokenizer(const Document *pDocument)
{
	return new RtfTokenizer(pDocument);
}

RtfTokenizer::RtfTokenizer(const Document *pDocument) :
	HtmlTokenizer(NULL, false)
{
	Document *pHtmlDocument = runHelperProgram(pDocument, "unrtf --nopict --html");
	if (pHtmlDocument != NULL)
	{
		if (parseHTML(pHtmlDocument) == true)
		{
			// Pass the result to the parent class
			Document *pStrippedDoc = new Document(pHtmlDocument->getTitle(),
				pHtmlDocument->getLocation(), pHtmlDocument->getType(),
				pHtmlDocument->getLanguage());
			pStrippedDoc->setData(m_state.m_text.c_str(), m_state.m_text.length());

			setDocument(pStrippedDoc);
		}

		delete pHtmlDocument;
	}
}

RtfTokenizer::~RtfTokenizer()
{
}
