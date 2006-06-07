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

#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <libintl.h>
#include <getopt.h>
#include <iostream>
#include <fstream>
#include <glibmm.h>
#include <glibmm/thread.h>
#include <glibmm/ustring.h>
#include <glibmm/convert.h>
#include <gtkmm/main.h>

#include "TokenizerFactory.h"
#include "Languages.h"
#include "MIMEScanner.h"
#include "XapianDatabase.h"
#include "XapianDatabaseFactory.h"
#include "QueryHistory.h"
#include "ViewHistory.h"
#include "DownloaderInterface.h"
#include "MozillaRenderer.h"
#include "config.h"
#include "NLS.h"
#include "PinotSettings.h"
#include "mainWindow.hh"

using namespace std;

static ofstream outputFile;
static streambuf *coutBuf = NULL;
static streambuf *cerrBuf = NULL;
static struct option g_longOptions[] = {
	{"help", 0, 0, 'h'},
	{"version", 0, 0, 'v'},
	{0, 0, 0, 0}
};

static void closeAll(void)
{
	cout << "Exiting..." << endl;

	// Save the settings
	PinotSettings &settings = PinotSettings::getInstance();
	if (settings.save() == false)
	{
		cerr << "Couldn't save configuration file" << endl;
	}

	// Close all indexes we may have opened
	XapianDatabaseFactory::closeAll();

	// Close the tokenizer libraries
	TokenizerFactory::unloadTokenizers();

	// Restore the stream buffers
	if (coutBuf != NULL)
	{
		cout.rdbuf(coutBuf);
	}
	if (cerrBuf != NULL)
	{
		cerr.rdbuf(cerrBuf);
	}
	outputFile.close();

	MozillaRenderer::shutdown();
	DownloaderInterface::shutdown();
	MIMEScanner::shutdown();
}

static void quitAll(int sigNum)
{
	cout << "Quitting..." << endl;

	Gtk::Main::quit();
}

int main(int argc, char **argv)
{
	string prefixDir(PREFIX);
	Glib::ustring errorMsg;
	struct sigaction newAction;
	int longOptionIndex = 0;

	// Look at the options
	int optionChar = getopt_long(argc, argv, "hv", g_longOptions, &longOptionIndex);
	while (optionChar != -1)
	{
		switch (optionChar)
		{
			case 'h':
				// Help
				cout << "pinot - A metasearch tool for the Free Desktop\n\n"
					<< "Usage: pinot [OPTIONS]\n\n"
					<< "Options:\n"
					<< "  -h, --help		display this help and exit\n"
					<< "  -v, --version		output version information and exit\n"
					<< "\nReport bugs to " << PACKAGE_BUGREPORT << endl;
				return EXIT_SUCCESS;
			case 'v':
				cout << "pinot - " << PACKAGE_STRING << "\n\n" 
					<< "This is free software.  You may redistribute copies of it under the terms of\n"
					<< "the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n"
					<< "There is NO WARRANTY, to the extent permitted by law." << endl;
				return EXIT_SUCCESS;
			default:
				return EXIT_FAILURE;
		}

		// Next option
		optionChar = getopt_long(argc, argv, "hv", g_longOptions, &longOptionIndex);
	}

#if defined(ENABLE_NLS)
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif //ENABLE_NLS

	MIMEScanner::initialize();
	DownloaderInterface::initialize();
	MozillaRenderer::initialize();
	Glib::thread_init();
	Gtk::Main m(&argc, &argv);

	// This will create the necessary directories on the first run
	PinotSettings &settings = PinotSettings::getInstance();

	string confDirectory = PinotSettings::getConfigurationDirectory();
	chdir(confDirectory.c_str());

	// Redirect cout and cerr to a file
	string logFileName = confDirectory;
	logFileName += "/pinot.log";
	outputFile.open(logFileName.c_str());
	coutBuf = cout.rdbuf();
	cerrBuf = cerr.rdbuf();
	cout.rdbuf(outputFile.rdbuf());
	cerr.rdbuf(outputFile.rdbuf());

	// Localize language names
	Languages::setIntlName(0, _("Danish"));
	Languages::setIntlName(1, _("Dutch"));
	Languages::setIntlName(2, _("English"));
	Languages::setIntlName(3, _("Finnish"));
	Languages::setIntlName(4, _("French"));
	Languages::setIntlName(5, _("German"));
	Languages::setIntlName(6, _("Italian"));
	Languages::setIntlName(7, _("Norwegian"));
	Languages::setIntlName(8, _("Portuguese"));
	Languages::setIntlName(9, _("Russian"));
	Languages::setIntlName(10, _("Spanish"));
	Languages::setIntlName(11, _("Swedish"));

	// Load search engines
	settings.loadSearchEngines(prefixDir + string("/share/pinot/engines"));
	settings.loadSearchEngines(confDirectory + string("/engines"));
	// Load tokenizer libraries, if any
	TokenizerFactory::loadTokenizers(prefixDir + string("/share/pinot/tokenizers"));
	TokenizerFactory::loadTokenizers(confDirectory + string("/tokenizers"));
	// Load the settings
	settings.load();

	// Catch interrupts
	sigemptyset(&newAction.sa_mask);
	newAction.sa_flags = 0;
	newAction.sa_handler = quitAll;
	sigaction(SIGINT, &newAction, NULL);
	sigaction(SIGQUIT, &newAction, NULL);

	// Ensure Xapian will be able to deal with internal indices
	XapianDatabase *pDb = XapianDatabaseFactory::getDatabase(settings.m_indexLocation, false);
	if ((pDb == NULL) ||
		(pDb->isOpen() == false))
	{
		errorMsg = _("Couldn't open index");
		errorMsg += " ";
		errorMsg += settings.m_indexLocation;
	}
	pDb = XapianDatabaseFactory::getDatabase(settings.m_mailIndexLocation, false);
	if ((pDb == NULL) ||
		(pDb->isOpen() == false))
	{
		errorMsg = _("Couldn't open index");
		errorMsg += " ";
		errorMsg += settings.m_mailIndexLocation;
	}

	// Do the same for the history database
	if ((settings.m_historyDatabase.empty() == true) ||
		(QueryHistory::create(settings.m_historyDatabase) == false) ||
		(ViewHistory::create(settings.m_historyDatabase) == false))
	{
		errorMsg = _("Couldn't create history database");
		errorMsg += " ";
		errorMsg += settings.m_historyDatabase;
	}

	atexit(closeAll);

	try
	{
		// Set an icon for all windows
		Gtk::Window::set_default_icon_from_file(prefixDir + string("/share/icons/hicolor/48x48/apps/pinot.png"));

		// Create and open the main dialog box
		mainWindow mainBox;
		if (errorMsg.empty() == false)
		{
			mainBox.set_status(errorMsg);
		}
		m.run(mainBox);
	}
	catch (const Glib::Exception &e)
	{
		cerr << e.what() << endl;
		return EXIT_FAILURE;
	}
	catch (...)
	{
		cerr << "Unknown exception" << endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
