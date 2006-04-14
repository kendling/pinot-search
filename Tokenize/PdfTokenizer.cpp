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

#include <string.h>
#include <iostream>

#include "PdfTokenizer.h"

using std::string;
using std::set;

/// This returns the MIME types supported by the library's tokenizer.
bool getTokenizerTypes(set<string> &types)
{
	types.clear();
	types.insert("application/pdf");

	return true;
}

/// This returns a pointer to a Tokenizer, allocated with new.
Tokenizer *getTokenizer(const Document *pDocument)
{
	return new PdfTokenizer(pDocument);
}

PdfTokenizer::PdfTokenizer(const Document *pDocument) :
	HtmlTokenizer(NULL)
{
	// Run pdftohtml
	Document *pOutputDocument = runHelperProgram(pDocument, "pdftohtml -stdout");
	if (pOutputDocument != NULL)
	{
		// Give the result to the parent class
		initialize(pOutputDocument);
	}
}

PdfTokenizer::PdfTokenizer() :
	HtmlTokenizer()
{
}

PdfTokenizer::~PdfTokenizer()
{
}
