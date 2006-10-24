/*
 *  Copyright 2006,2006 Fabrice Colin
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <string>

#include "HtmlTokenizer.h"
#include "XmlTokenizer.h"
#include "UnknownTypeTokenizer.h"
#include "TokenizerFactory.h"
#include "StringManip.h"

using namespace std;

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		cerr << "Usage: " << argv[0] << " <file name> RAWDATA|STRIP|LISTLINKS|LISTTOKENS|META=<meta tag name>|SUBSTR=<sub-string> [TYPE=<type>]" << endl;
		return EXIT_FAILURE;
	}

	Document doc;
	if (doc.setDataFromFile(argv[1]) == false)
	{
		cerr << "Couldn't load " << argv[1] << " !" << endl;
		return EXIT_FAILURE;
	}

	Tokenizer *pTokens = NULL;
	// How shall we get the tokenizer ?
	if ((argc >= 4) &&
		(strncmp(argv[3], "TYPE=", 5) == 0))
	{
		// By type
		pTokens = TokenizerFactory::getTokenizerByType(argv[3] + 5, &doc);
	}
	else
	{
		pTokens = TokenizerFactory::getTokenizer(argv[1], &doc);
	}

	if (pTokens == NULL)
	{
		cerr << "Couldn't obtain tokenizer for " << argv[1] << " !" << endl;
		return EXIT_FAILURE;
	}

	HtmlTokenizer *pHtmlTokens = dynamic_cast<HtmlTokenizer*>(pTokens);

	const Document *pDoc = NULL;
	unsigned int length = 0;

	if (strncmp(argv[2], "RAWDATA", 5) == 0)
	{
		// Call the base class's method
		pDoc = pTokens->Tokenizer::getDocument();
		if (pDoc != NULL)
		{
			cout << "Raw text is :" << endl;
			cout << pDoc->getData(length) << endl;
		}
	}
	else if (strncmp(argv[2], "STRIP", 5) == 0)
	{
		const char *pData = doc.getData(length);

		cout << "Stripped text is :" << endl;
		cout << XmlTokenizer::stripTags(string(pData, length)) << endl;
	}
	else if ((strncmp(argv[2], "LISTLINKS", 9) == 0) &&
		(pHtmlTokens != NULL))
	{
		// Get a list of links
		set<Link> &links = pHtmlTokens->getLinks();
		if (links.empty() == false)
		{
			set<Link>::iterator iter;
			for(iter = links.begin(); iter != links.end(); iter++)
			{
				cout << "Found link \"" << iter->m_name << "\" to " << iter->m_url << endl;
			}
		}
		else
		{
			cout << "No links were found in " << argv[1] << endl;
		}
	}
	else if (strncmp(argv[2], "LISTTOKENS", 10) == 0)
	{
		string token;
		cout << "Tokens are :" << endl;
		while (pTokens->nextToken(token) == true)
		{
			cout << token << endl;
		}
	}
	else if ((strncmp(argv[2], "META=", 5) == 0) &&
		(pHtmlTokens != NULL))
	{
		string metaTag = argv[2] + 5;
		cout << "Meta tag " << metaTag << " was set to " << pHtmlTokens->getMetaTag(metaTag) << endl;
	}
	else if ((strncmp(argv[2], "SUBSTR=", 7) == 0) &&
		(pHtmlTokens != NULL))
	{
		string subString = argv[2] + 7;

		pDoc = pTokens->getDocument();
		if (pDoc != NULL)
		{
			cout << "Replaced sub-string " << subString << " with 'Hello'" << endl;
			cout << StringManip::replaceSubString(pDoc->getData(length), subString, "Hello") << endl;
		}
	}

	delete pTokens;

	return EXIT_SUCCESS;
}
