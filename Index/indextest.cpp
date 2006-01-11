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
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <set>
#include <iostream>

#include "Document.h"
#include "TokenizerFactory.h"
#include "XapianDatabaseFactory.h"
#include "XapianIndex.h"

using namespace std;

int main(int argc, char **argv)
{
	bool success = false;

	if (argc < 3)
	{
		cerr << "Usage: " << argv[0] << " <index type> <database name> CHECK|CREATE|HASURL=<url>|INDEX=<file name>" << endl;
		return EXIT_FAILURE;
	}

	// Check database ?
	if (strncasecmp(argv[3], "CHECK", 5) == 0)
	{
		XapianIndex index(argv[2]);

		if (index.isGood() == true)
		{
			cout << "Index has " << index.getDocumentsCount() << " document(s)" << endl;
			success = true;
		}
	}
	// Create database ?
	else if (strncasecmp(argv[3], "CREATE", 6) == 0)
	{
		if (XapianDatabaseFactory::getDatabase(argv[2], false) != NULL)
		{
			success = true;
		}
	}
	// Look for an URL ?
	else if (strncasecmp(argv[3], "HASURL", 6) == 0)
	{
		XapianIndex index(argv[2]);

		if ((index.isGood() == true) &&
			(index.hasDocument(argv[3] + 7) > 0))
		{
			success = true;
		}
	}
	// Index a file ?
	else if (strncasecmp(argv[3], "INDEX", 5) == 0)
	{
		struct stat fileStat;
		string fileName(argv[3] + 6);

		if ((stat(fileName.c_str(), &fileStat) == 0) &&
			(S_ISREG(fileStat.st_mode)))
		{
			char *buffer = new char[fileStat.st_size + 1];
			int fd = open(fileName.c_str(), O_RDONLY);
			// Read the file
			ssize_t readBytes = read(fd, buffer, fileStat.st_size);
			if (readBytes > 0)
			{
				// Assume file is HTML...
				Document doc(fileName, fileName, "text/html", "");
				doc.setData(buffer, readBytes);
				if (doc.isBinary() == false)
				{
					set<string> labels;
					unsigned int docId = 0;

					Tokenizer *pTokens = TokenizerFactory::getTokenizer(fileName, &doc);
					if (pTokens != NULL)
					{
						// Ignore index type, use a XapianIndex
						XapianIndex index(argv[2]);
						index.setStemmingMode(IndexInterface::STORE_BOTH);
						if (index.indexDocument(*pTokens, labels, docId) == true)
						{
							cout << "Added " << fileName << " to index, document" << docId << endl;
							success = true;
						}

						delete pTokens;
					}
				}

				delete[] buffer;
			}
		}
	}

	XapianDatabaseFactory::closeAll();

	if (success == true)
	{
		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}
