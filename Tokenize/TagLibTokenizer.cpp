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

#include <fileref.h>
#include <tfile.h>
#include <tag.h>

#include "Url.h"
#include "TagLibTokenizer.h"

using std::string;
using std::set;

/// This returns the MIME types supported by the library's tokenizer.
bool getTokenizerTypes(set<string> &types)
{
	types.clear();
	types.insert("audio/mpeg");
	types.insert("audio/x-mp3");
	types.insert("application/ogg");
	types.insert("audio/x-flac+ogg");
	types.insert("audio/x-flac");

	return true;
}

/// This returns the data needs of the provided Tokenizer(s).
int getTokenizerDataNeeds(void)
{
	return Tokenizer::ALL_DOCUMENTS;
}

/// This returns a pointer to a Tokenizer, allocated with new.
Tokenizer *getTokenizer(const Document *pDocument)
{
	return new TagLibTokenizer(pDocument);
}

TagLibTokenizer::TagLibTokenizer(const Document *pDocument) :
	Tokenizer(NULL),
	m_pTagDocument(NULL)
{
	if (pDocument != NULL)
	{
		Url urlObj(pDocument->getLocation());
		string pseudoContent;

		if ((urlObj.isLocal() == true) &&
			(urlObj.getFile().empty() == false))
		{
			string location(urlObj.getLocation());
			string trackTitle;

			location += "/";
			location += urlObj.getFile();

			TagLib::FileRef fileRef(location.c_str(), false);
			if (fileRef.isNull() == false)
			{
				TagLib::Tag *pTag = fileRef.tag();
				if ((pTag != NULL) &&
					(pTag->isEmpty() == false))
				{
					char yearStr[64];

					trackTitle = pTag->title().to8Bit(); 
					trackTitle += " ";
					trackTitle += pTag->artist().to8Bit();

					pseudoContent = trackTitle;
					pseudoContent += " ";
					pseudoContent += pTag->album().to8Bit();
					pseudoContent += " ";
					pseudoContent += pTag->comment().to8Bit();
					pseudoContent += " ";
					pseudoContent += pTag->genre().to8Bit();
					snprintf(yearStr, 64, " %u", pTag->year());
					pseudoContent += yearStr;
				}
			}
			else
			{
				trackTitle = pseudoContent = pDocument->getTitle();
			}

			m_pTagDocument = new Document(trackTitle, pDocument->getLocation(),
				pDocument->getType(), pDocument->getLanguage());
			m_pTagDocument->setData(pseudoContent.c_str(), pseudoContent.length());
			m_pTagDocument->setTimestamp(pDocument->getTimestamp());
			m_pTagDocument->setSize(pDocument->getSize());

			// Give the result to the parent class
			setDocument(m_pTagDocument);
		}
	}
}

TagLibTokenizer::~TagLibTokenizer()
{
	if (m_pTagDocument != NULL)
	{
		delete m_pTagDocument;
	}
}
