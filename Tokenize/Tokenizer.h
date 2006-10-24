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

#ifndef _TOKENIZER_H
#define _TOKENIZER_H

#include <string>

#include "Document.h"

class Tokenizer
{
	public:
		Tokenizer(const Document *pDocument);
		virtual ~Tokenizer();

		/// Converts a document using an helper program.
		static Document *runHelperProgram(const Document *pDocument,
			const std::string &programName, const std::string &arguments = "");

		/// Returns a pointer to the document being tokenized.
		virtual const Document *getDocument(void);

		/// Returns the next token; false if all tokens consumed.
		virtual bool nextToken(std::string &token);

		/// Rewinds the tokenizer.
		virtual void rewind(void);

	protected:
		const Document *m_pDocument;
		unsigned int m_currentPos;

		void setDocument(const Document *pDocument);

	private:
		Tokenizer(const Tokenizer &other);
		Tokenizer& operator=(const Tokenizer& other);

};

#endif // _TOKENIZER_H
