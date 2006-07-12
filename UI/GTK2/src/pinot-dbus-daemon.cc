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
#include <glibmm/object.h>
extern "C"
{
#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
}
#include <glibmm/main.h>

#include "DocumentInfo.h"
#include "TokenizerFactory.h"
#include "Languages.h"
#include "MIMEScanner.h"
#include "XapianDatabase.h"
#include "XapianDatabaseFactory.h"
#include "QueryHistory.h"
#include "ViewHistory.h"
#include "DownloaderInterface.h"
#include "config.h"
#include "NLS.h"
#include "PinotSettings.h"

using namespace std;

static ofstream outputFile;
static streambuf *coutBuf = NULL;
static streambuf *cerrBuf = NULL;
static struct option g_longOptions[] = {
	{"help", 0, 0, 'h'},
	{"version", 0, 0, 'v'},
	{0, 0, 0, 0}
};
static const char *g_pinotDBusService = "de.berlios.Pinot";
static const char *g_pinotDBusObjectPath = "/de/berlios/Pinot";
static DBusHandlerResult objectPathHandler(DBusConnection *pConnection, DBusMessage *pMessage, void *pData);
static DBusObjectPathVTable g_callVTable = {
	NULL,
	objectPathHandler,
	NULL,
};
static Glib::RefPtr<Glib::MainLoop> g_refMainLoop;

static void closeAll(void)
{
	cout << "Exiting..." << endl;

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

	DownloaderInterface::shutdown();
	MIMEScanner::shutdown();
}

static void quitAll(int sigNum)
{
	if (g_refMainLoop->is_running() == true)
	{
		cout << "Quitting..." << endl;

		g_refMainLoop->quit();
	}
}

static DBusHandlerResult objectPathHandler(DBusConnection *pConnection, DBusMessage *pMessage, void *pData)
{
#ifdef DEBUG
	cout << "objectPathHandler: called" << endl;
#endif
	if (dbus_message_is_method_call(pMessage, g_pinotDBusService, "Index") == TRUE)
	{
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static DBusHandlerResult messageBusFilter(DBusConnection *pConnection, DBusMessage *pMessage, void *pData)
{
	DBusMessage *pReply = NULL;
	bool processedMessage = false;

#ifdef DEBUG
	cout << "messageBusFilter: called" << endl;
#endif

	// Are we about to be disconnected ?
	if (dbus_message_is_signal(pMessage, DBUS_INTERFACE_LOCAL, "Disconnected") == TRUE)
	{
		// FIXME: is this legal ?
		Glib::RefPtr<Glib::MainLoop> g_refMainLoop = Glib::MainLoop::create(true);

		// Tell the main loop to quit
		g_refMainLoop->quit();

		return DBUS_HANDLER_RESULT_HANDLED;
	}
	else if (dbus_message_is_method_call(pMessage, g_pinotDBusService, "Index") == TRUE)
	{
		DBusError error;
		char *pTitle = NULL;
		char *pLocation = NULL;
		char *pType = NULL;
		char *pLanguage = NULL;
		char *pLabel = NULL;
		dbus_uint64_t docId = 0;

		// Simple types are returned as const references and don't need to be freed
		dbus_error_init(&error);
		if (dbus_message_get_args(pMessage, &error,
			DBUS_TYPE_STRING, &pTitle,
			DBUS_TYPE_STRING, &pLocation,
			DBUS_TYPE_STRING, &pType,
			DBUS_TYPE_STRING, &pLanguage,
			DBUS_TYPE_STRING, &pLabel,
			DBUS_TYPE_UINT64, &docId,
			DBUS_TYPE_INVALID) == TRUE)
		{
			DocumentInfo docInfo(pTitle, pLocation, pType, pLanguage);

#ifdef DEBUG
			cout << "messageBusFilter: received " << pTitle << ", " << pLocation
				<< ", " << pType << ", " << pLanguage << ", " << pLabel
				<< ", " << docId << endl;
#endif
			// FIXME: index docInfo

			// Prepare the reply
			pReply = dbus_message_new_method_return(pMessage);
			if (pReply != NULL)
			{
				dbus_message_append_args(pReply,
					DBUS_TYPE_UINT64, &docId, DBUS_TYPE_INVALID);
			}
		}
		else
		{
			// Use the error message as reply
			pReply = dbus_message_new_error(pMessage, error.name, error.message);
		}
		dbus_error_free(&error);

		processedMessage = true;
	}
#ifdef DEBUG
	else cout << "messageBusFilter: message for foreign object" << endl;
#endif

	// Send a reply ?
	if (pReply != NULL)
	{
		if (dbus_message_get_no_reply(pMessage) == FALSE)
		{
			dbus_connection_send(pConnection, pReply, NULL);
#ifdef DEBUG
			cout << "messageBusFilter: sent reply" << endl;
#endif
		}
#ifdef DEBUG
		else cout << "messageBusFilter: no need to send a reply" << endl;
#endif

		dbus_message_unref(pReply);
	}

	if (processedMessage == true)
	{
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

int main(int argc, char **argv)
{
	string prefixDir(PREFIX);
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
				cout << "pinot-dbus-daemon - D-Bus search and index daemon\n\n"
					<< "Usage: pinot-dbus-daemon [OPTIONS]\n\n"
					<< "Options:\n"
					<< "  -h, --help		display this help and exit\n"
					<< "  -v, --version		output version information and exit\n"
					<< "\nReport bugs to " << PACKAGE_BUGREPORT << endl;
				return EXIT_SUCCESS;
			case 'v':
				cout << "pinot-dbus-daemon - " << PACKAGE_STRING << "\n\n" 
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
	Glib::thread_init();

	// This will create the necessary directories on the first run
	PinotSettings &settings = PinotSettings::getInstance();

	string confDirectory = PinotSettings::getConfigurationDirectory();
	chdir(confDirectory.c_str());

	// Redirect cout and cerr to a file
	string logFileName = confDirectory;
	logFileName += "/pinot-dbus-daemon.log";
	outputFile.open(logFileName.c_str());
	coutBuf = cout.rdbuf();
	cerrBuf = cerr.rdbuf();
	cout.rdbuf(outputFile.rdbuf());
	cerr.rdbuf(outputFile.rdbuf());

	// Localize language names
	Languages::setIntlName(0, _("Unknown"));
	Languages::setIntlName(1, _("Danish"));
	Languages::setIntlName(2, _("Dutch"));
	Languages::setIntlName(3, _("English"));
	Languages::setIntlName(4, _("Finnish"));
	Languages::setIntlName(5, _("French"));
	Languages::setIntlName(6, _("German"));
	Languages::setIntlName(7, _("Italian"));
	Languages::setIntlName(8, _("Norwegian"));
	Languages::setIntlName(9, _("Portuguese"));
	Languages::setIntlName(10, _("Russian"));
	Languages::setIntlName(11, _("Spanish"));
	Languages::setIntlName(12, _("Swedish"));

	// Load search engines
	settings.loadSearchEngines(prefixDir + string("/share/pinot/engines"));
	settings.loadSearchEngines(confDirectory + string("/engines"));
	// Load tokenizer libraries, if any
	TokenizerFactory::loadTokenizers(prefixDir + string("/share/pinot/tokenizers"));
	TokenizerFactory::loadTokenizers(confDirectory + string("/tokenizers"));
	// Load the settings
	settings.loadGlobal(prefixDir + string("/share/pinot/globalconfig.xml"));
	settings.load();

	// Catch interrupts
	sigemptyset(&newAction.sa_mask);
	newAction.sa_flags = 0;
	newAction.sa_handler = quitAll;
	sigaction(SIGINT, &newAction, NULL);
	sigaction(SIGQUIT, &newAction, NULL);

#if 0
	// Ensure Xapian will be able to deal with internal indices
	XapianDatabase *pDb = XapianDatabaseFactory::getDatabase(settings.m_indexLocation, false);
	if ((pDb == NULL) ||
		(pDb->isOpen() == false))
	{
		cerr << _("Couldn't open index") << " " << settings.m_indexLocation << endl;;
	}
	pDb = XapianDatabaseFactory::getDatabase(settings.m_mailIndexLocation, false);
	if ((pDb == NULL) ||
		(pDb->isOpen() == false))
	{
		cerr << _("Couldn't open index") << " " << settings.m_mailIndexLocation << endl;
	}
#endif

	// Do the same for the history database
	if ((settings.m_historyDatabase.empty() == true) ||
		(QueryHistory::create(settings.m_historyDatabase) == false) ||
		(ViewHistory::create(settings.m_historyDatabase) == false))
	{
		cerr << _("Couldn't create history database") << " " << settings.m_historyDatabase << endl;
	}
	else
	{
		QueryHistory queryHistory(settings.m_historyDatabase);
		ViewHistory viewHistory(settings.m_historyDatabase);
		time_t timeNow = time(NULL);

		// Expire items older than a month
		queryHistory.expireItems(timeNow - 2592000);
		viewHistory.expireItems(timeNow - 2592000);
	}

	atexit(closeAll);

	// Initialize the D-Bus thread system
	dbus_g_thread_init();

	GError *pError = NULL;
	DBusGConnection *pBus = dbus_g_bus_get(DBUS_BUS_SESSION, &pError);
	if (pBus == NULL)
	{
		if (pError != NULL)
		{
			cerr << "Couldn't open connection: " << pError->message << endl;
			g_error_free(pError);
		}

		return EXIT_FAILURE;
	}

	// Listen for messages from all objects
	DBusConnection *pConnection = dbus_g_connection_get_connection(pBus);
	if (pConnection != NULL)
	{
		DBusError error;

		dbus_error_init(&error);
		dbus_connection_set_exit_on_disconnect(pConnection, FALSE);
		dbus_connection_setup_with_g_main(pConnection, NULL);

		dbus_connection_add_filter(pConnection, messageBusFilter, NULL, NULL);
		if (dbus_error_is_set(&error) == FALSE)
		{
			// Request to be identified by this name
			// FIXME: flags are currently broken ?
			dbus_bus_request_name(pConnection, g_pinotDBusService, 0, &error);
			if ((dbus_error_is_set(&error) == FALSE) &&
				(dbus_connection_register_object_path(pConnection, g_pinotDBusObjectPath,
					&g_callVTable, NULL) == TRUE))
			{
				// Run the main loop
				g_refMainLoop = Glib::MainLoop::create();
				g_refMainLoop->run();
			}
			else
			{
				cerr << "Couldn't register object path: " << pError->message << endl;
			}
		}
		else
		{
			cerr << "Couldn't add filter: " << pError->message << endl;
		}
		dbus_error_free(&error);
	}
	dbus_connection_close(pConnection);
	dbus_g_connection_unref(pBus);

	return EXIT_SUCCESS;
}
