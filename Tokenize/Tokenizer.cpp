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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <sys/types.h>
#include <ctype.h>
#include <unistd.h>
#include <iostream>
#include <fstream>

#include "CommandLine.h"
#include "Tokenizer.h"

//#define DEBUG_TOKENIZER

using std::cout;
using std::cerr;
using std::endl;
using std::string;

Tokenizer::Tokenizer(const Document *pDocument, bool splitOnSpaces) :
	m_pDocument(NULL),
	m_splitOnSpaces(splitOnSpaces),
	m_currentPos(0)
{
	setDocument(pDocument);
	rewind();
}

Tokenizer::~Tokenizer()
{
	// The document is owned by the caller
}

void Tokenizer::setDocument(const Document *pDocument)
{
	m_pDocument = pDocument;
}

/**
  * Returns a pointer to the document being tokenized.
  * This may not be the document passed to the constructor.
  */
const Document *Tokenizer::getDocument(void)
{
	return m_pDocument;
}

/// Returns the next token; false if all tokens consumed.
bool Tokenizer::nextToken(string &token)
{
	bool bStarted = false;
	unsigned int dataLength;
	unsigned int pos = m_currentPos;

	if (m_pDocument == NULL)
	{
#ifdef DEBUG_TOKENIZER
		cout << "Tokenizer::nextToken: no document" << endl;
#endif
		return false;
	}

	const char *pData = m_pDocument->getData(dataLength);
	if ((pData == NULL) ||
		(dataLength == 0))
	{
#ifdef DEBUG_TOKENIZER
		cout << "Tokenizer::nextToken: no data" << endl;
#endif
		return false;
	}

#ifdef DEBUG_TOKENIZER
	if (pos == 0)
	{
		ofstream tokData("TokenizerData.txt");
		tokData << pData << endl;
		tokData.close();
	}
#endif
	while (pos < dataLength)
	{
		if (((m_splitOnSpaces == false) && (isalnum(pData[pos]) != 0)) ||
			((m_splitOnSpaces == true) && (isspace(pData[pos]) == 0)))
		{
			if (bStarted == false)
			{
				// This starts the new token
				token = pData[pos];
				bStarted = true;
			}
			else
			{
				// Append to token
				token += pData[pos];
			}
		}
		else
		{
			if (bStarted == true)
			{
#ifdef DEBUG_TOKENIZER
				cout << "Tokenizer::nextToken: returning current token " << token << endl;
#endif
				// Return the current token
				break;
			}
			// Else keep going until we find an alnum
		}

		// Next
		pos++;
	}
	m_currentPos = pos;

	return bStarted;
}

/// Rewinds the tokenizer.
void Tokenizer::rewind(void)
{
	m_currentPos = 0;
}
