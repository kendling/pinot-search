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

#ifndef _SUMMARIZER_H
#define _SUMMARIZER_H

#include <string>

class Summarizer
{
	public:
		Summarizer(const std::string &language, unsigned int wordsCount);
		virtual ~Summarizer();

		/// Attempts to summarize the document in wordsCount words.
		std::string summarize(const char *pText, unsigned int textLen);

		/// Gets the document's title, as determined by summarize().
		std::string getTitle(void) const;

	protected:
		static unsigned int m_maxTextSize;
		static unsigned int m_maxSummarySize;
		unsigned int m_wordsCount;
		std::string m_dictionaryCode;
		std::string m_title;

	private:
		Summarizer(const Summarizer &other);
		Summarizer &operator=(const Summarizer &other);

};

#endif // _SUMMARIZER_H
