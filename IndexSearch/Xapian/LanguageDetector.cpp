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

#include <strings.h>
#include <sys/time.h>
#include <iostream>
#include <utility>
#include <cstring>

#include "config.h"
extern "C"
{
#ifdef HAVE_LIBTEXTCAT_TEXTCAT_H
#include <libtextcat/textcat.h>
#else
#include <textcat.h>
#endif
}

#include "StringManip.h"
#include "Timer.h"
#include "LanguageDetector.h"
#include "config.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;
using std::min;

unsigned int LanguageDetector::m_maxTextSize = 1000;

LanguageDetector::LanguageDetector()
{
}

LanguageDetector::~LanguageDetector()
{
}

/**
  * Attempts to guess the language.
  * Returns a list of candidates, or "unknown" if detection failed.
  */
void LanguageDetector::guessLanguage(const char *pData, unsigned int dataLength,
			std::vector<std::string> &candidates)
{
	string confFile(SYSCONFDIR);
	char *textCatVersion = textcat_Version();
#ifdef HAVE_TEXTCAT_CAT
	const char *catResults[10];
#endif

	candidates.clear();

	// What configuration file should we use ?
	confFile += "/pinot/";
#ifdef DEBUG
	cout << "LanguageDetector::guessLanguage: detected " << textCatVersion << endl;
#endif
	if (strncasecmp(textCatVersion, "TextCat 3", 9) == 0)
	{
		// Version 3
		confFile += "textcat3_conf.txt";
	}
	else
	{
		confFile += "textcat_conf.txt";
	}

	// Initialize
	void *td = textcat_Init(confFile.c_str());
	if (td == NULL)
	{
		candidates.push_back("unknown");
		return;
	}

	// Classify
#ifdef DEBUG
	Timer timer;
	timer.start();
	cout << "LanguageDetector::guessLanguage: starting" << endl;
#endif
#ifdef HAVE_TEXTCAT_CAT
	unsigned int resultNum = textcat_Cat(td, pData,
		min(dataLength, m_maxTextSize), catResults, 10);
	if (resultNum == 0 )
	{
		candidates.push_back("unknown");
	}
	else
	{
		for (unsigned int i=0; i<resultNum; ++i)
		{
			string language(StringManip::toLowerCase(catResults[i]));

			// Remove the charset information
			string::size_type dashPos = language.find('-');
			if (dashPos != string::npos)
			{
				language.resize(dashPos);
			}
#ifdef DEBUG
			cout << "LanguageDetector::guessLanguage: found language " << language << endl;
#endif
			candidates.push_back(language);
		}
	}
#else
	const char *languages = textcat_Classify(td, pData,
		min(dataLength, m_maxTextSize));
	if (languages == NULL)
	{
		candidates.push_back("unknown");
	}
	else
	{
		// The output will be either SHORT, or UNKNOWN or a list of languages in []
		if ((strncasecmp(languages, "SHORT", 5) == 0) ||
			(strncasecmp(languages, "UNKNOWN", 7) == 0))
		{
			candidates.push_back("unknown");
		}
		else
		{
			string languageList(languages);
			string::size_type lastPos = 0, pos = languageList.find_first_of("[");

			while (pos != string::npos)
			{
				++pos;
				lastPos = languageList.find_first_of("]", pos);
				if (lastPos == string::npos)
				{
					break;
				}

				string language(StringManip::toLowerCase(languageList.substr(pos, lastPos - pos)));
				// Remove the charset information
				string::size_type dashPos = language.find('-');
				if (dashPos != string::npos)
				{
					language.resize(dashPos);
				}
#ifdef DEBUG
				cout << "LanguageDetector::guessLanguage: found language " << language << endl;
#endif
				candidates.push_back(language);

				// Next
				pos = languageList.find_first_of("[", lastPos);
			}
		}
	}
#endif
#ifdef DEBUG
	cout << "LanguageDetector::guessLanguage: language guessing took "
		<< timer.stop() << " ms" << endl;
#endif

	// Close the descriptor
	textcat_Done(td);
}
