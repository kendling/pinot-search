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
#include <iostream>
#include <fstream>
#include <glibmm.h>
#include <glibmm/thread.h>
#include <glibmm/ustring.h>
#include <glibmm/convert.h>
#include <gtkmm/main.h>

#include "TokenizerFactory.h"
#include "Languages.h"
#include "XapianDatabase.h"
#include "XapianDatabaseFactory.h"
#include "QueryHistory.h"
#include "ViewHistory.h"
#include "NeonDownloader.h"
#include "MozillaRenderer.h"
#include "config.h"
#include "NLS.h"
#include "PinotSettings.h"
#include "mainWindow.hh"

using namespace std;

static ofstream outputFile;
static streambuf *coutBuf = NULL;
static streambuf *cerrBuf = NULL;

static void closeAll(void)
{
	cout << "Exiting..." << endl;

	// Save the settings
	PinotSettings &settings = PinotSettings::getInstance();
	if (settings.save() == false)
	{
		cerr << _("Couldn't save configuration file") << endl;
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
	NeonDownloader::shutdown();
}

static void quitAll(int sigNum)
{
	cout << "Quitting..." << endl;

	Gtk::Main::quit();
}

int main(int argc, char **argv)
{
	struct sigaction newAction;

#if defined(ENABLE_NLS)
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	textdomain (GETTEXT_PACKAGE);
#endif //ENABLE_NLS

	NeonDownloader::initialize();
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

	// Load the settings
	settings.load();
	settings.loadSearchEngines("/usr/share/pinot/engines");
	settings.loadSearchEngines(confDirectory + string("/engines"));
	// Load tokenizer libraries, if any
	TokenizerFactory::loadTokenizers("/usr/share/pinot/tokenizers");
	TokenizerFactory::loadTokenizers(confDirectory + string("/tokenizers"));

	// Catch interrupts
	sigemptyset(&newAction.sa_mask);
	newAction.sa_flags = 0;
	newAction.sa_handler = quitAll;
	sigaction(SIGINT, &newAction, NULL);
	sigaction(SIGQUIT, &newAction, NULL);

	// Ensure Xapian will be able to deal with internal indices
	if (XapianDatabaseFactory::getDatabase(settings.m_indexLocation, false) == NULL)
	{
		cerr << _("Index") << " " << settings.m_indexLocation << " "
			<< _("is not valid, please check") << endl;
	}
	if (XapianDatabaseFactory::getDatabase(settings.m_mailIndexLocation, false) == NULL)
	{
		cerr << _("Index") << " " << settings.m_mailIndexLocation << " "
			<< _("is not valid, please check") << endl;
	}

	// Do the same for the history database
	if ((settings.m_historyDatabase.empty() == true) ||
		(QueryHistory::create(settings.m_historyDatabase) == false) ||
		(ViewHistory::create(settings.m_historyDatabase) == false))
	{
		cerr << _("History database") << " " << settings.m_historyDatabase << " "
			<< _("couldn't be created") << endl;
	}

	atexit(closeAll);

	// Create and open the main dialog box
	mainWindow mainBox;
	m.run(mainBox);

	return EXIT_SUCCESS;
}
