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

#include <sys/time.h>
#include <map>
#include <iostream>
#include <utility>

#include <ots/libots.h>

#include "Languages.h"
#include "StringManip.h"
#include "Timer.h"
#include "Summarizer.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::map;
using std::min;

unsigned int Summarizer::m_maxTextSize = 500000;

Summarizer::Summarizer(const std::string &language, unsigned int wordsCount) :
	m_wordsCount(wordsCount),
	m_dictionaryCode("en")
{
	string lang = StringManip::toLowerCase(language);

	// Look up the language code
	for (unsigned int count = 0; count < Languages::m_count; ++count)
	{
		if (lang == Languages::m_names[count])
		{
			m_dictionaryCode = Languages::m_codes[count];
		}
	}
}

Summarizer::~Summarizer()
{
}

/// Attempts to summarize the document in wordsCount words.
string Summarizer::summarize(const char *pText, unsigned int textLen)
{
	if ((pText == NULL) ||
		(textLen == 0))
	{
		return NULL;
	}

	m_title.clear();

	// OTS may take too much time with long documents
	if (textLen < m_maxTextSize)
	{
		unsigned char *pSummary = NULL;
		size_t outputLen = 0;
#ifdef DEBUG
		Timer timer;
		timer.start();
#endif

		// Create a new article
		OtsArticle *pArticle = ots_new_article();
		if ((pArticle != NULL) &&
			(ots_load_xml_dictionary(pArticle, m_dictionaryCode.c_str())))
		{
			ots_parse_stream((const unsigned char*)pText, textLen, pArticle);

			ots_grade_doc(pArticle);
			ots_highlight_doc_words(pArticle, m_wordsCount);

			// Summarize
			pSummary = ots_get_doc_text(pArticle, &outputLen);
#ifdef DEBUG
			cout << "Summarizer::summarize: summarized to " << outputLen << " bytes in "
				<< timer.stop() << " us " << endl;
#endif

			// Get the title before freeing the article
			if (pArticle->title != NULL)
			{
				m_title = pArticle->title;
			}
			ots_free_article(pArticle);
		}

		if (pSummary != NULL)
		{
			string sum((const char *)pSummary, outputLen);

			free(pSummary);

			return sum;
		}
	}
	else
	{
		unsigned int arbitraryLen = min(5 * m_wordsCount, m_maxTextSize / 1000);

		// This is totally arbitray
		return string(pText, arbitraryLen);
	}

	return "";
}

/// Gets the document's title, as determined by summarize().
string Summarizer::getTitle(void) const
{
	return m_title;
}
