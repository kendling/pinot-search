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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>

#include "UnknownTypeTokenizer.h"

using namespace std;

UnknownTypeTokenizer::UnknownTypeTokenizer(const Document *pDocument) :
	Tokenizer(NULL)
{
	char inTemplate[15] = "/tmp/tokXXXXXX";
	char outTemplate[15] = "/tmp/tokXXXXXX";

	int inFd = mkstemp(inTemplate);
	int outFd = mkstemp(outTemplate);
	if ((inFd != -1) &&
		(outFd != -1))
	{
		unsigned int dataLength = 0;
		const char *pData = pDocument->getData(dataLength);

		// Save the data into a temporary file
		if (write(inFd, (const void*)pData, dataLength) != -1)
		{
			// Run strings against it to extract printable characters
			string cmdLine = "strings --bytes=6 ";
			cmdLine += inTemplate;
			cmdLine += ">";
			cmdLine += outTemplate;
			
			if (system(cmdLine.c_str()) != -1)
			{
				struct stat fileStat;

				// Read the output
				if ((stat(outTemplate, &fileStat) != -1) &&
					(S_ISREG(fileStat.st_mode)))
				{
					unsigned int total, bytes;
					char *content = new char[fileStat.st_size + 1];

					total = bytes = read(outFd, (void*)content, fileStat.st_size);
					while ((bytes > 0) &&
						(total < fileStat.st_size))
					{
						bytes = read(outFd, (void*)content, fileStat.st_size - total);
						total += bytes;
					}

					// Pass the result to the parent class
					Document *pStrippedDoc = new Document(pDocument->getTitle(),
						pDocument->getLocation(), pDocument->getType(),
						pDocument->getLanguage());
					pStrippedDoc->setData(content, bytes);
					setDocument(pStrippedDoc);
#ifdef DEBUG
					cout << "UnknownTypeTokenizer::ctor: set " << bytes << " bytes of data" << endl;
#endif

					delete[] content;
				}
			}
		}
	}

	close(outFd);
	close(inFd);

	if ((unlink(outTemplate) != 0) ||
		(unlink(inTemplate) != 0))
	{
#ifdef DEBUG
		cerr << "UnknownTypeTokenizer::ctor: couldn't delete temporary files" << endl;
#endif
	}
}

UnknownTypeTokenizer::~UnknownTypeTokenizer()
{
	if (m_pDocument != NULL)
	{
		// This should have been set by setDocument(),
		// called by the constructor
		delete m_pDocument;
	}
}
