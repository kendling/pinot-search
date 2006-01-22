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

#include "OpenSearchParser.h"
#include "SherlockParser.h"

using namespace std;

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		cerr << "Usage: " << argv[0] << " SHERLOCK|OPENSEARCH <file name> [MIN]" << endl;
		return EXIT_FAILURE;
	}

	struct stat fileStat;
	if ((stat(argv[2], &fileStat) == 0) &&
		(S_ISREG(fileStat.st_mode)))
	{
		bool minParser = false;

		if ((argc >= 4) &&
			(strncasecmp(argv[3], "MIN", 3) == 0))
		{
			minParser = true;
		}

		if (strncasecmp(argv[1], "SHERLOCK", 8) == 0)
		{
			char *buffer = new char[fileStat.st_size + 1];
			int fd = open(argv[2], O_RDONLY);

			// Read the file
			ssize_t readBytes = read(fd, buffer, fileStat.st_size);
			if (readBytes == -1)
			{
				cerr << "Couldn't read " << argv[2] << " !" << endl;
				return EXIT_FAILURE;
			}

			// Put that data into a document
			Document doc;
			doc.setData(buffer, readBytes);
			delete[] buffer;

			SherlockParser parser(&doc);
			if (parser.parse(minParser) == true)
			{
				cout << "Successfully parsed " << argv[2] << endl;
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
		else if (strncasecmp(argv[1], "OPENSEARCH", 10) == 0)
		{
			OpenSearchParser parser(argv[2]);

			if (parser.parse(minParser) == true)
			{
				cout << "Successfully parsed " << argv[2] << endl;
			}

			SearchPluginProperties &properties = parser.getProperties();

			cout << "Plugin " << properties.m_name << ": " << properties.m_description << endl;
			cout << "Channel: " << properties.m_channel << endl;
			cout << "URL: " << properties.m_baseUrl << endl;
			cout << "Input parameters are:" << endl;
			for (map<SearchPluginProperties::Parameter, string>::iterator iter = properties.m_parameters.begin();
				iter != properties.m_parameters.end(); ++iter)
			{
				cout << iter->second << "=" << iter->first << endl;
			}
			cout << "Remainder:" << properties.m_parametersRemainder << endl;
		}
	}
	else
	{
		cerr << "Couldn't stat " << argv[2] << " !" << endl;
	}

	return EXIT_SUCCESS;
}
