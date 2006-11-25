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

#include <string.h>
#include <iostream>

#include "OpenDocumentTokenizer.h"

using std::string;
using std::set;

/// This returns the MIME types supported by the library's tokenizer.
bool getTokenizerTypes(set<string> &types)
{
	types.clear();
	types.insert("application/vnd.sun.xml.writer");
	types.insert("application/vnd.sun.xml.writer.template");
	types.insert("application/vnd.sun.xml.calc");
	types.insert("application/vnd.sun.xml.calc.template");
	types.insert("application/vnd.sun.xml.draw");
	types.insert("application/vnd.sun.xml.draw.template");
	types.insert("application/vnd.sun.xml.impress");
	types.insert("application/vnd.sun.xml.impress.template");
	types.insert("application/vnd.sun.xml.writer.global");
	types.insert("application/vnd.sun.xml.math");
	types.insert("application/vnd.oasis.opendocument.chart");
	types.insert("application/vnd.oasis.opendocument.database");
	types.insert("application/vnd.oasis.opendocument.formula");
	types.insert("application/vnd.oasis.opendocument.graphics");
	types.insert("application/vnd.oasis.opendocument.graphics-template");
	types.insert("application/vnd.oasis.opendocument.image");
	types.insert("application/vnd.oasis.opendocument.presentation");
	types.insert("application/vnd.oasis.opendocument.presentation-template");
	types.insert("application/vnd.oasis.opendocument.spreadsheet");
	types.insert("application/vnd.oasis.opendocument.spreadsheet-template");
	types.insert("application/vnd.oasis.opendocument.text");
	types.insert("application/vnd.oasis.opendocument.text-master");
	types.insert("application/vnd.oasis.opendocument.text-template");
	types.insert("application/vnd.oasis.opendocument.text-web");

	return true;
}

/// This returns the data needs of the provided Tokenizer(s).
int getTokenizerDataNeeds(void)
{
	return Tokenizer::ALL_BUT_FILES;
}

/// This returns a pointer to a Tokenizer, allocated with new.
Tokenizer *getTokenizer(const Document *pDocument)
{
	return new OpenDocumentTokenizer(pDocument);
}

OpenDocumentTokenizer::OpenDocumentTokenizer(const Document *pDocument) :
	XmlTokenizer(NULL)
{
	Document *pXmlDocument = runHelperProgram(pDocument, "unzip -p", "content.xml");
	if (pXmlDocument != NULL)
	{
		unsigned int length = 0;
		const char *data = pXmlDocument->getData(length);

		if ((data != NULL) &&
			(length > 0))
		{
			// Remove XML tags
			string strippedData = parseXML(data);

			// Pass the result to the parent class
			m_pStrippedDocument = new Document(pDocument->getTitle(),
				pDocument->getLocation(), pDocument->getType(),
				pDocument->getLanguage());
			m_pStrippedDocument->setData(strippedData.c_str(), strippedData.length());
			m_pStrippedDocument->setTimestamp(pDocument->getTimestamp());
			m_pStrippedDocument->setSize(pDocument->getSize());

			setDocument(m_pStrippedDocument);
		}

		delete pXmlDocument;
	}
	// FIXME: unzip meta.xml and extract document information
}

OpenDocumentTokenizer::~OpenDocumentTokenizer()
{
	// ~XmlTokenizer will delete m_pStrippedDocument
}
