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
#include <glibmm/shell.h>
#include <glibmm/spawn.h>
#include <iostream>

#include "CommandLine.h"
#include "Url.h"

using std::cout;
using std::endl;
using std::string;
using std::vector;

CommandLine::CommandLine()
{
}

CommandLine::~CommandLine()
{
}

/// Runs a command asynchronously.
bool CommandLine::runAsync(const MIMEAction &action, const vector<string> &arguments)
{
	string commandLine(action.m_exec);

	if (commandLine.empty() == true)
	{
		return false;
	}

	if (arguments.empty() == false)
	{
		vector<string>::const_iterator firstArg = arguments.begin();
		Url firstUrl(*firstArg);
		bool foundParam = false;

		// Expand parameters
		// We assume that arguments are URLs
		string::size_type paramPos = commandLine.find('%');
		while ((paramPos != string::npos) &&
				(paramPos + 1 < commandLine.length()))
		{
			string replacement;

			foundParam = true;
			switch (commandLine[paramPos + 1])
			{
				// Single parameter arguments
				case 'u':
					replacement = *firstArg;
					break;
				case 'f':
					if (firstUrl.isLocal() == true)
					{
						replacement = firstUrl.getLocation();
						replacement += "/";
						replacement += firstUrl.getFile();
					}
					break;
				case 'd':
					if (firstUrl.isLocal() == true)
					{
						replacement = firstUrl.getLocation();
					}
					break;
				case 'n':
					if (firstUrl.isLocal() == true)
					{
						replacement = firstUrl.getFile();
					}
				// Multiple parameters arguments
				case 'U':
					for (vector<string>::const_iterator argIter = firstArg; argIter != arguments.end(); ++argIter)
					{
						if (replacement.empty() == false)
						{
							replacement += " ";
						}
						replacement += *argIter;
					}
					break;
				case 'F':
					for (vector<string>::const_iterator argIter = firstArg; argIter != arguments.end(); ++argIter)
					{
						Url argUrl(*argIter);

						if (argUrl.isLocal() == true)
						{
							if (replacement.empty() == false)
							{
								replacement += " ";
							}
							replacement += argUrl.getLocation();
							replacement += "/";
							replacement += argUrl.getFile();
						}
					}
					break;
				case 'D':
					for (vector<string>::const_iterator argIter = firstArg; argIter != arguments.end(); ++argIter)
					{
						Url argUrl(*argIter);

						if (argUrl.isLocal() == true)
						{
							if (replacement.empty() == false)
							{
								replacement += " ";
							}
							replacement += argUrl.getLocation();
						}
					}
					break;
				case 'N':
					for (vector<string>::const_iterator argIter = firstArg; argIter != arguments.end(); ++argIter)
					{
						Url argUrl(*argIter);

						if (argUrl.isLocal() == true)
						{
							if (replacement.empty() == false)
							{
								replacement += " ";
							}
							replacement += argUrl.getFile();
						}
					}
					break;
				// Other parameters
				case 'i':
					replacement = action.m_icon;
					break;
				case 'c':
					// FIXME: this should be the "translated name"
					replacement = action.m_name;
					break;
				case 'k':
					replacement = action.m_location;
					break;
				case 'v':
					replacement = action.m_device;
					break;
				default:
					break;
			}

			commandLine.replace(paramPos, 2, Glib::shell_quote(replacement));

			// Next
			paramPos = commandLine.find('%', paramPos + 1);
		}

		if (foundParam == false)
		{
			// If no parameter was found, assume %f
			commandLine += *firstArg;
		}
	}
#ifdef DEBUG
		cout << "CommandLine::runAsync: spawning '" << commandLine << "'" << endl;
#endif

	Glib::spawn_command_line_async(commandLine);

	return true;
}
