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

#ifndef _LANGUAGE_DETECTOR_H
#define _LANGUAGE_DETECTOR_H

#include <string>
#include <vector>

class LanguageDetector
{
	public:
		LanguageDetector();
		virtual ~LanguageDetector();

		/**
		  * Attempts to guess the language.
		  * Returns a list of candidates, or "unknown" if detection failed.
		  */
		void guessLanguage(const char *pData, unsigned int dataLength,
			std::vector<std::string> &candidates);

	private:
		LanguageDetector(const LanguageDetector &other);
		LanguageDetector &operator=(const LanguageDetector &other);

};

#endif // _LANGUAGE_DETECTOR_H
