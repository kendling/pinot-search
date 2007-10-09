/*
 *  Copyright 2007 Fabrice Colin
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
 
#include <getopt.h>
#include <sys/types.h>
#include <pwd.h>
#include <iostream>
#include <string>
#include <set>
#include "config.h"
extern "C"
{
#if DBUS_NUM_VERSION < 1000000
#define DBUS_API_SUBJECT_TO_CHANGE
#endif
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
}

#include "StringManip.h"
#include "MIMEScanner.h"
#include "DBusXapianIndex.h"
#include "XapianDatabaseFactory.h"

using namespace std;

static struct option g_longOptions[] = {
	{"get", 0, 0, 'g'},
	{"help", 0, 0, 'h'},
	{"list", 0, 0, 'l'},
	{"set", 1, 0, 's'},
	{"version", 0, 0, 'v'},
	{0, 0, 0, 0}
};


static void printLabels(const set<string> &labels)
{
	cout << "Labels: ";
	for (set<string>::const_iterator labelIter = labels.begin();
		labelIter != labels.end(); ++labelIter)
	{
		cout << "[" << *labelIter << "]";
	}
	cout << endl;
}

static string getHomeDirectory(void)
{
	struct passwd *pPasswd = getpwuid(geteuid());

	if ((pPasswd != NULL) &&
		(pPasswd->pw_dir != NULL))
	{
		return pPasswd->pw_dir;
	}
	else
	{
		char *homeDir = getenv("HOME");
		if (homeDir != NULL)
		{
			return homeDir;
		}
	}

	return "~";
}

static void printHelp(void)
{
	// Help
	cout << "pinot-label - Label files from the command-line\n\n"
		<< "Usage: pinot-label [OPTIONS] [FILE]\n\n"
		<< "Options:\n"
		<< "  -g, --get                 get the labels list for the given file\n"
		<< "  -h, --help                display this help and exit\n"
		<< "  -l, --list                list known labels\n"
		<< "  -s, --set                 set labels on the given file\n"
		<< "  -v, --version             output version information and exit\n\n";
	cout << "Examples:\n"
		<< "pinot-label --get /home/fabrice/Documents/Bozo.txt\n\n"
		<< "pinot-label --list\n\n"
		<< "pinot-label --set \"[Clowns][Fun][My Hero]\" /home/fabrice/Documents/Bozo.txt\n\n"
		<< "Report bugs to " << PACKAGE_BUGREPORT << endl;
}

int main(int argc, char **argv)
{
	set<string> labels;
	string labelsString;
	int longOptionIndex = 0;
	unsigned int docId = 0;
	bool getLabels = false, getDocumentLabels = false, setDocumentLabels = false, success = false;

	// Look at the options
	int optionChar = getopt_long(argc, argv, "ghls:v", g_longOptions, &longOptionIndex);
	while (optionChar != -1)
	{
		set<string> engines;

		switch (optionChar)
		{
			case 'g':
				getDocumentLabels = true;
				break;
			case 'h':
				printHelp();
				return EXIT_SUCCESS;
			case 'l':
				getLabels = true;
				break;
			case 's':
				setDocumentLabels = true;
				if (optarg != NULL)
				{
					labelsString = optarg;
				}
				break;
			case 'v':
				cout << "pinot-label - " << PACKAGE_STRING << "\n\n"
					<< "This is free software.  You may redistribute copies of it under the terms of\n"
					<< "the GNU General Public License <http://www.gnu.org/licenses/old-licenses/gpl-2.0.html>.\n"
					<< "There is NO WARRANTY, to the extent permitted by law." << endl;
				return EXIT_SUCCESS;
			default:
				return EXIT_FAILURE;
		}

		// Next option
		optionChar = getopt_long(argc, argv, "ghls:v", g_longOptions, &longOptionIndex);
	}

	if (argc == 1)
	{
		printHelp();
		return EXIT_SUCCESS;
	}

	if ((setDocumentLabels == true) &&
		(labelsString.empty() == true))
	{
		cerr << "Incorrect parameters" << endl;
		return EXIT_FAILURE;
	}

	// Initialize GType
	g_type_init();

	MIMEScanner::initialize("", "");

	string indexLocation(getHomeDirectory() + "/.pinot/daemon");
	DBusXapianIndex index(indexLocation);
	if (index.isGood() == false)
	{
		cerr << "Couldn't obtain index for " << indexLocation << endl;

		XapianDatabaseFactory::closeAll();
		MIMEScanner::shutdown();

		return EXIT_FAILURE;
	}

	if ((getDocumentLabels == true) ||
		(setDocumentLabels == true))
	{
		string fileParam(argv[optind]);

		docId = index.hasDocument(string("file://") + fileParam);
		if (docId == 0)
		{
			cerr << "File is not indexed" << endl;

			XapianDatabaseFactory::closeAll();
			MIMEScanner::shutdown();

			return EXIT_FAILURE;
		}
	}

	if (getLabels == true)
	{
		if (index.getLabels(labels, true) == true)
		{
			printLabels(labels);

			success = true;
		}
	}

	if (getDocumentLabels == true)
	{
		labels.clear();

		if (index.getDocumentLabels(docId, labels, true) == true)
		{
			printLabels(labels);

			success = true;
		}
	}

	if (setDocumentLabels == true)
	{
		string::size_type endPos = 0;
		string label(StringManip::extractField(labelsString, "[", "]", endPos));

		labels.clear();

		// Parse labels
		while (label.empty() == false)
		{
			labels.insert(Url::unescapeUrl(label));

			if (endPos == string::npos)
			{
				break;
			}
			label = StringManip::extractField(labelsString, "[", "]", endPos);
		}

#ifdef DEBUG
		printLabels(labels);
#endif
		success = index.setDocumentLabels(docId, labels);
	}

	XapianDatabaseFactory::closeAll();
	MIMEScanner::shutdown();

	// Did whatever operation we carried out succeed ?
	if (success == true)
	{
		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}
