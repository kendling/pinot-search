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

/// Runs a command synchronously.
bool CommandLine::runSync(const string &commandLine, string &output)
{
	int exitStatus = 0;

	if (commandLine.empty() == true)
	{
		return false;
	}

	Glib::spawn_command_line_sync(commandLine, &output, NULL, &exitStatus);
	if (exitStatus == 0)
	{
		return true;
	}
#ifdef DEBUG
	cout << "CommandLine::runSync: exit status is " << exitStatus << endl;
#endif

	return false;
}

/// Runs a command asynchronously.
bool CommandLine::runAsync(const MIMEAction &action, const vector<string> &arguments)
{
	string commandLine(action.m_exec);

	if (commandLine.empty() == true)
	{
		return false;
	}
#ifdef DEBUG
	cout << "CommandLine::runAsync: " << arguments.size() << " arguments for application '"
		<< action.m_exec << "'" << endl;
#endif

	// We may have to spawn several copies of the same program if it doesn't support multiple arguments
	vector<string>::const_iterator firstArg = arguments.begin();
	while (firstArg != arguments.end())
	{
		Url firstUrl(*firstArg);
		bool foundParam = false;
		bool noArgument = true;
		bool oneArgumentOnly = true;

		// Expand parameters
		// We assume that all arguments are full-blown URLs
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
					noArgument = false;
					break;
				case 'f':
					if (firstUrl.getProtocol() != "file")
					{
						replacement = firstUrl.getLocation();
						replacement += "/";
						replacement += firstUrl.getFile();
					}
					noArgument = false;
					break;
				case 'd':
					if (firstUrl.getProtocol() != "file")
					{
						replacement = firstUrl.getLocation();
					}
					noArgument = false;
					break;
				case 'n':
					if (firstUrl.getProtocol() != "file")
					{
						replacement = firstUrl.getFile();
					}
					noArgument = false;
					break;
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
					noArgument = oneArgumentOnly = false;
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
					noArgument = oneArgumentOnly = false;
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
					noArgument = oneArgumentOnly = false;
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
					noArgument = oneArgumentOnly = false;
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

			string shellReplacement(Glib::shell_quote(replacement));
			commandLine.replace(paramPos, 2, shellReplacement);

			// Next
			paramPos = commandLine.find('%', paramPos + shellReplacement.length());
		}

		if (foundParam == false)
		{
			// If no parameter was found, assume %f
			if (firstUrl.getProtocol() != "file")
			{
				commandLine += firstUrl.getLocation();
				commandLine += "/";
				commandLine += firstUrl.getFile();
			}
		}

#ifdef DEBUG
		cout << "CommandLine::runAsync: spawning '" << commandLine << "'" << endl;
#endif
		Glib::spawn_command_line_async(commandLine);

		if ((noArgument == true) ||
			(oneArgumentOnly == false))
		{
			// No or all arguments were processed
			break;
		}
		else
		{
			// Only the first argument was processed
			commandLine = action.m_exec;
			++firstArg;
		}

	}

	return true;
}
