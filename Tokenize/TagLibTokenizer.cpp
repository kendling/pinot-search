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

#include <fileref.h>
#include <tfile.h>
#include <tag.h>

#include "Url.h"
#include "TagLibTokenizer.h"

/**
  * This returns the MIME type supported by the library's tokenizer.
  * The character string is allocated with new[].
  */
char *getTokenizerType(unsigned int typeNum)
{
	if (typeNum == 0)
	{
		char *pType = new char[11];
		strncpy(pType, "audio/mpeg", 10);
		pType[10] = '\0';
		return pType;
	}
	else if (typeNum == 1)
	{
		char *pType = new char[12];
		strncpy(pType, "audio/x-mp3", 11);
		pType[11] = '\0';
		return pType;
	}
	else if (typeNum == 2)
	{
		char *pType = new char[16];
		strncpy(pType, "application/ogg", 15);
		pType[15] = '\0';
		return pType;
	}
	else if (typeNum == 3)
	{
		char *pType = new char[17];
		strncpy(pType, "audio/x-flac+ogg", 16);
		pType[16] = '\0';
		return pType;
	}
	else if (typeNum == 4)
	{
		char *pType = new char[13];
		strncpy(pType, "audio/x-flac", 12);
		pType[12] = '\0';
		return pType;
	}

	return NULL;
}

/// This returns a pointer to a Tokenizer, allocated with new.
Tokenizer *getTokenizer(const Document *pDocument)
{
	return new TagLibTokenizer(pDocument);
}

TagLibTokenizer::TagLibTokenizer(const Document *pDocument) :
	Tokenizer(NULL)
{
	if (pDocument != NULL)
	{
		Document *pPseudoDocument = NULL;
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

			if ((fileRef.isNull() == false) &&
				(fileRef.file()->isOpen() == true))
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

			pPseudoDocument = new Document(trackTitle, pDocument->getLocation(),
				pDocument->getType(), pDocument->getLanguage());
			pPseudoDocument->setData(pseudoContent.c_str(), pseudoContent.length());

			// Give the result to the parent class
			setDocument(pPseudoDocument);
		}
	}
}

TagLibTokenizer::~TagLibTokenizer()
{
	if (m_pDocument != NULL)
	{
		// This should have been set by setDocument()
		delete m_pDocument;
	}
}
