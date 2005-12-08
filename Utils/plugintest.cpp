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

#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <string>

#include "PluginParser.h"

using namespace std;

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		cerr << "Usage: " << argv[0] << " <file name> [MIN]" << endl;
		return EXIT_FAILURE;
	}

	struct stat fileStat;
	if ((stat(argv[1], &fileStat) == 0) &&
		(S_ISREG(fileStat.st_mode)))
	{
		char *buffer = new char[fileStat.st_size + 1];
		int fd = open(argv[1], O_RDONLY);
		// Read the file
		ssize_t readBytes = read(fd, buffer, fileStat.st_size);
		if (readBytes == -1)
		{
			cerr << "Couldn't read " << argv[1] << " !" << endl;
			return EXIT_FAILURE;
		}

		// Put that data into a document
		Document doc;
		doc.setData(buffer, readBytes);
		delete[] buffer;

		bool minParser = false;
		PluginParser parser(&doc);

		if ((argc >= 3) &&
			(strncasecmp(argv[2], "MIN", 3) == 0))
		{
			minParser = true;
		}

		if (parser.parse(minParser) == true)
		{
			cout << "Successfully parsed " << argv[1] << endl;
		}

		PluginProperties &properties = parser.getProperties();

		cout << "SEARCH parameters are :" << endl;
		for (map<string, string>::iterator iter = properties.m_searchParams.begin();
			iter != properties.m_searchParams.end(); ++iter)
		{
			cout << iter->first << "=" << iter->second << endl;
		}
		cout << "End of SEARCH parameters" << endl;

		cout << "INPUT items are :" << endl;
		for (map<string, string>::iterator iter = properties.m_inputItems.begin();
			iter != properties.m_inputItems.end(); ++iter)
		{
			if (iter->first != properties.m_userInput)
			{
				cout << iter->first << "=" << iter->second << endl;
			}
			else
			{
				cout << iter->first << " USER" << endl;
			}
		}
		cout << "NEXT " << properties.m_nextInput << "=" << properties.m_nextFactor << endl;
		cout << "End of INPUT items" << endl;

		cout << "INTERPRET parameters are :" << endl;
		for (map<string, string>::iterator iter = properties.m_interpretParams.begin();
			iter != properties.m_interpretParams.end(); ++iter)
		{
			cout << iter->first << "=" << iter->second << endl;
		}
		cout << "End of INTERPRET parameters" << endl;

	}
	else
	{
		cerr << "Couldn't stat " << argv[1] << " !" << endl;
	}

	return EXIT_SUCCESS;
}
