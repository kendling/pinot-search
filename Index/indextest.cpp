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
	if (argc < 4)
	{
		cerr << "Usage: " << argv[0] << " <index type> <database name> <file name>|CHECK|CREATE" << endl;
		return EXIT_FAILURE;
	}

	// Check database ?
	if (strncasecmp(argv[3], "CHECK", 5) == 0)
	{
		if (XapianDatabaseFactory::getDatabase(argv[2], true) != NULL)
		{
			XapianIndex index(argv[2]);
			cout << "Index has " << index.getDocumentsCount() << " document(s)" << endl;

			return EXIT_SUCCESS;
		}

		return EXIT_FAILURE;
	}
	// Create database ?
	else if (strncasecmp(argv[3], "CREATE", 6) == 0)
	{
		if (XapianDatabaseFactory::getDatabase(argv[2], false) != NULL)
		{
			return EXIT_SUCCESS;
		}

		return EXIT_FAILURE;
	}

	struct stat fileStat;
	if ((stat(argv[3], &fileStat) == 0) &&
		(S_ISREG(fileStat.st_mode)))
	{
		char *buffer = new char[fileStat.st_size + 1];
		int fd = open(argv[3], O_RDONLY);
		// Read the file
		ssize_t readBytes = read(fd, buffer, fileStat.st_size);
		if (readBytes == -1)
		{
			cerr << "Couldn't read " << argv[3] << " !" << endl;
			return EXIT_FAILURE;
		}

		// Assume file is HTML...
		Document doc(argv[3], argv[3], "text/html", "");
		doc.setData(buffer, readBytes);
		if (doc.isBinary() == true)
		{
			cerr << argv[3] << " is binary !" << endl;
		}
		else
		{
			set<string> labels;
			unsigned int docId = 0;

			Tokenizer *pTokens = TokenizerFactory::getTokenizer(argv[3], &doc);
			if (pTokens == NULL)
			{
				cerr << "Couldn't obtain tokenizer for " << argv[3] << " !" << endl;
				return EXIT_FAILURE;
			}

			// Ignore index type, use a XapianIndex
			XapianIndex index(argv[2]);
			index.setStemmingMode(IndexInterface::STORE_BOTH);
			if (index.indexDocument(*pTokens, labels, docId) == false)
			{
				cerr << "Couldn't index " << argv[3] << " !" << endl;
			}
			else
			{
				cout << "Added " << argv[3] << " to index, document" << docId << endl;
			}

			delete pTokens;
		}

		delete[] buffer;
	}
	else
	{
		cerr << "Couldn't stat " << argv[3] << " !" << endl;
	}

	return EXIT_SUCCESS;
}
