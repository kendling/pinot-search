/*
 *  Copyright 2005-2009 Fabrice Colin
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <stdlib.h>
#include <libintl.h>
#include <getopt.h>
#ifdef HAVE_LINUX_SCHED_H
#include <linux/sched.h>
#include <sched.h>
#else
#include <sys/time.h>
#include <sys/resource.h>
#endif
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string.h>
#include <strings.h>
#include <sigc++/sigc++.h>
#include <glibmm.h>
#include <glibmm/thread.h>
#include <glibmm/ustring.h>
#include <glibmm/miscutils.h>
#include <glibmm/convert.h>

#include "NLS.h"
#include "FilterFactory.h"
#include "Languages.h"
#include "MIMEScanner.h"
#include "ModuleFactory.h"
#include "ActionQueue.h"
#include "CrawlHistory.h"
#include "MetaDataBackup.h"
#include "QueryHistory.h"
#include "ViewHistory.h"
#include "DownloaderInterface.h"
#include "DaemonState.h"
#include "PinotSettings.h"
#include "ServerThreads.h"
#include "UniqueApplication.h"

using namespace std;

static ofstream g_outputFile;
static string g_pidFileName;
static streambuf *g_coutBuf = NULL;
static streambuf *g_cerrBuf = NULL;
static struct option g_longOptions[] = {
	{"fullscan", 0, 0, 'f'},
	{"help", 0, 0, 'h'},
	{"ignore-version", 0, 0, 'i'},
	{"priority", 1, 0, 'p'},
	{"reindex", 0, 0, 'r'},
	{"version", 0, 0, 'v'},
	{0, 0, 0, 0}
};
static const char *g_pinotDBusService = "de.berlios.Pinot";
static const char *g_pinotDBusObjectPath = "/de/berlios/Pinot";
static void unregisteredHandler(DBusConnection *pConnection, void *pData);
static DBusHandlerResult messageHandler(DBusConnection *pConnection, DBusMessage *pMessage, void *pData);
static DBusObjectPathVTable g_callVTable = {
	(DBusObjectPathUnregisterFunction)unregisteredHandler,
        (DBusObjectPathMessageFunction)messageHandler,
	NULL,
};
static Glib::RefPtr<Glib::MainLoop> g_refMainLoop;

static void closeAll(void)
{
	cout << "Exiting..." << endl;

	// Close everything
	ModuleFactory::unloadModules();

	// Restore the stream buffers
	if (g_coutBuf != NULL)
	{
		cout.rdbuf(g_coutBuf);
	}
	if (g_cerrBuf != NULL)
	{
		cerr.rdbuf(g_cerrBuf);
	}
	g_outputFile.close();
	if (g_pidFileName.empty() == false)
	{
		unlink(g_pidFileName.c_str());
	}

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

static DBusHandlerResult filterHandler(DBusConnection *pConnection, DBusMessage *pMessage, void *pData)
{
	DaemonState *pServer = (DaemonState *)pData;

#ifdef DEBUG
	cout << "filterHandler: called" << endl;
#endif
	// Are we about to be disconnected ?
	if (dbus_message_is_signal(pMessage, DBUS_INTERFACE_LOCAL, "Disconnected") == TRUE)
	{
#ifdef DEBUG
		cout << "filterHandler: received Disconnected" << endl;
#endif
		if (pServer != NULL)
		{
			pServer->set_flag(DaemonState::DISCONNECTED);
		}
		quitAll(0);
	}
	else if (dbus_message_is_signal(pMessage, DBUS_INTERFACE_DBUS, "NameOwnerChanged") == TRUE)
	{
#ifdef DEBUG
		cout << "filterHandler: received NameOwnerChanged" << endl;
#endif
	}
	// The first two signals are specified by the freedesktop.org Power Management spec v0.1 and v0.2
	else if ((dbus_message_is_signal(pMessage, "org.freedesktop.PowerManagement", "BatteryStateChanged") == TRUE) ||
		(dbus_message_is_signal(pMessage, "org.freedesktop.PowerManagement", "OnBatteryChanged") == TRUE) ||
		(dbus_message_is_signal(pMessage, "org.gnome.PowerManager", "OnAcChanged") == TRUE))
	{
		DBusError error;
		gboolean onBattery = FALSE;

#ifdef DEBUG
		cout << "filterHandler: received OnBatteryChanged" << endl;
#endif
		dbus_error_init(&error);
		if ((dbus_message_get_args(pMessage, &error,
			DBUS_TYPE_BOOLEAN, &onBattery,
			DBUS_TYPE_INVALID) == TRUE) &&
			(pData != NULL))
		{
			if (dbus_message_is_signal(pMessage, "org.gnome.PowerManager", "OnAcChanged") == TRUE)
			{
				// This tells us if we are on AC, not on battery
				if (onBattery == TRUE)
				{
					onBattery = FALSE;
				}
				else
				{
					onBattery = TRUE;
				}
			}

			if (onBattery == TRUE)
			{
				// We are now on battery
				if (pServer != NULL)
				{
					pServer->set_flag(DaemonState::ON_BATTERY);
					pServer->stop_crawling();
				}

				cout << "System is now on battery" << endl;
			}
			else
			{
				// Back on-line
				if (pServer != NULL)
				{
					pServer->reset_flag(DaemonState::ON_BATTERY);
					pServer->start_crawling();
				}

				cout << "System is now on AC" << endl;
			}
		}
		dbus_error_free(&error);
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void unregisteredHandler(DBusConnection *pConnection, void *pData)
{
#ifdef DEBUG
	cout << "unregisteredHandler: called" << endl;
#endif
}

static DBusHandlerResult messageHandler(DBusConnection *pConnection, DBusMessage *pMessage, void *pData)
{
	DaemonState *pServer = (DaemonState *)pData;

	if ((pConnection != NULL) &&
		(pMessage != NULL))
	{
		dbus_connection_ref(pConnection);
		dbus_message_ref(pMessage);

		if (pServer != NULL)
		{
			DBusServletInfo *pInfo = new DBusServletInfo(pConnection, pMessage);

			pServer->start_thread(new DBusServletThread(pServer, pInfo));
		}
	}

	return DBUS_HANDLER_RESULT_HANDLED;
}

static bool getBatteryState(DBusGConnection *pBus, const string &name, const string &path,
	const string &method, gboolean &result)
{
	bool callSuccess = true;

	if ((pBus == NULL) ||
		(name.empty() == true) ||
		(path.empty() == true) ||
		(method.empty() == true))
	{
		return false;
	}

	DBusGProxy *pBusProxy = dbus_g_proxy_new_for_name(pBus, name.c_str(),
		path.c_str(), name.c_str());
	if (pBusProxy == NULL)
	{
		return false;
	}

	GError *pError = NULL;
	if (dbus_g_proxy_call(pBusProxy, method.c_str(), &pError,
		G_TYPE_INVALID,
		G_TYPE_BOOLEAN, &result,
		G_TYPE_INVALID) == FALSE)
	{
		if (pError != NULL)
		{
			cerr << "Couldn't get battery state: " << pError->message << endl;
			g_error_free(pError);
		}

		callSuccess = false;
	}

	g_object_unref(pBusProxy);

	return callSuccess;
}

int main(int argc, char **argv)
{
	int longOptionIndex = 0, priority = 15;
	bool resetHistory = false;
	bool resetLabels = false;
	bool fullScan = false;
	bool reindex = false;
	bool ignoreVersion = false;

	// Look at the options
	int optionChar = getopt_long(argc, argv, "fhip:rv", g_longOptions, &longOptionIndex);
	while (optionChar != -1)
	{
		switch (optionChar)
		{
			case 'f':
				fullScan = true;
				break;
			case 'h':
				// Help
				cout << "pinot-dbus-daemon - D-Bus search and index daemon\n\n"
					<< "Usage: pinot-dbus-daemon [OPTIONS]\n\n"
					<< "Options:\n"
					<< "  -f, --fullscan	force a full scan\n"
					<< "  -h, --help		display this help and exit\n"
					<< "  -i, --ignore-version	ignore the index version number\n"
					<< "  -p, --priority	set the daemon's priority (default 15)\n"
					<< "  -r, --reindex		force a reindex\n"
					<< "  -v, --version		output version information and exit\n"
					<< "\nReport bugs to " << PACKAGE_BUGREPORT << endl;
				return EXIT_SUCCESS;
			case 'i':
				ignoreVersion = true;
				break;
			case 'p':
				if (optarg != NULL)
				{
					int newPriority = atoi(optarg);
					if ((newPriority >= -20) &&
						(newPriority < 20))
					{
						priority = newPriority;
					}
				}
				break;
			case 'r':
				reindex = true;
				break;
			case 'v':
				cout << "pinot-dbus-daemon - " << PACKAGE_STRING << "\n\n" 
					<< "This is free software.  You may redistribute copies of it under the terms of\n"
					<< "the GNU General Public License <http://www.gnu.org/licenses/old-licenses/gpl-2.0.html>.\n"
					<< "There is NO WARRANTY, to the extent permitted by law." << endl;
				return EXIT_SUCCESS;
			default:
				return EXIT_FAILURE;
		}

		// Next option
		optionChar = getopt_long(argc, argv, "fhip:rv", g_longOptions, &longOptionIndex);
	}

#if defined(ENABLE_NLS)
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif //ENABLE_NLS

	// Initialize threads support before doing anything else
	if (Glib::thread_supported() == false)
	{
		Glib::thread_init();
	}
	// Initialize the GType and the D-Bus thread system
	g_type_init();
#if DBUS_NUM_VERSION > 1000000
	dbus_threads_init_default();
#endif
	dbus_g_thread_init();

	g_refMainLoop = Glib::MainLoop::create();
	Glib::set_application_name("Pinot DBus Daemon");

	// This should make Xapian use Flint rather than Quartz
	Glib::setenv("XAPIAN_PREFER_FLINT", "1");

	// Make the locale follow the environment variables
	setlocale(LC_ALL, "");
	char *pLocale = setlocale(LC_ALL, NULL);
	if (pLocale != NULL)
	{
		string locale(pLocale);

		if (locale != "C")
		{
			bool appendUTF8 = false;

			string::size_type pos = locale.find_last_of(".");
			if ((pos != string::npos) &&
				((strcasecmp(locale.substr(pos).c_str(), ".utf8") != 0) &&
				(strcasecmp(locale.substr(pos).c_str(), ".utf-8") != 0)))
			{
				locale.resize(pos);
				appendUTF8 = true;
			}

			if (appendUTF8 == true)
			{
				locale += ".UTF-8";

				pLocale = setlocale(LC_ALL, locale.c_str());
				if (pLocale != NULL)
				{
#ifdef DEBUG
					cout << "Changed locale to " << pLocale << endl;
#endif
				}
			}
		}
	}

	// Make sure only one instance runs
	UniqueApplication uniqueApp("de.berlios.PinotDBusDaemon");
	string confDirectory(PinotSettings::getConfigurationDirectory());
	g_pidFileName = confDirectory + "/pinot-dbus-daemon.pid";
	if (chdir(confDirectory.c_str()) == 0)
	{
		if (uniqueApp.isRunning(g_pidFileName, "pinot-dbus-daemon") == true)
		{
			return EXIT_SUCCESS;
		}

		// Redirect cout and cerr to a file
		string fileName(confDirectory);
		fileName += "/pinot-dbus-daemon.log";
		g_outputFile.open(fileName.c_str());
		g_coutBuf = cout.rdbuf();
		g_cerrBuf = cerr.rdbuf();
		cout.rdbuf(g_outputFile.rdbuf());
		cerr.rdbuf(g_outputFile.rdbuf());
	}
	else
	{
		// We can't rely on the PID file
		if (uniqueApp.isRunning() == true)
		{
			return EXIT_SUCCESS;
		}
	}

	// This will create the necessary directories on the first run
	PinotSettings &settings = PinotSettings::getInstance();
	// This is the daemon so disable client-side code 
	settings.enableClientMode(false);

	// Initialize utility classes
	if (MIMEScanner::initialize(PinotSettings::getHomeDirectory() + "/.local",
		string(SHARED_MIME_INFO_PREFIX)) == false)
	{
		cerr << "Couldn't load MIME settings" << endl;
	}
	DownloaderInterface::initialize();
	// Load filter libraries, if any
	Dijon::FilterFactory::loadFilters(string(LIBDIR) + "/pinot/filters");
	Dijon::FilterFactory::loadFilters(confDirectory + "/filters");
	// Load backends, if any
	ModuleFactory::loadModules(string(LIBDIR) + "/pinot/backends");
	ModuleFactory::loadModules(confDirectory + "/backends");

	// Localize language names
	Languages::setIntlName(0, _("Unknown"));
	Languages::setIntlName(1, _("Danish"));
	Languages::setIntlName(2, _("Dutch"));
	Languages::setIntlName(3, _("English"));
	Languages::setIntlName(4, _("Finnish"));
	Languages::setIntlName(5, _("French"));
	Languages::setIntlName(6, _("German"));
	Languages::setIntlName(7, _("Hungarian"));
	Languages::setIntlName(8, _("Italian"));
	Languages::setIntlName(9, _("Norwegian"));
	Languages::setIntlName(10, _("Portuguese"));
	Languages::setIntlName(11, _("Romanian"));
	Languages::setIntlName(12, _("Russian"));
	Languages::setIntlName(13, _("Spanish"));
	Languages::setIntlName(14, _("Swedish"));
	Languages::setIntlName(15, _("Turkish"));

	// Load the settings
	settings.load(PinotSettings::LOAD_ALL);

	// Catch interrupts
#ifdef HAVE_SIGACTION
	struct sigaction newAction;
	sigemptyset(&newAction.sa_mask);
	newAction.sa_flags = 0;
	newAction.sa_handler = quitAll;
	sigaction(SIGINT, &newAction, NULL);
	sigaction(SIGQUIT, &newAction, NULL);
	sigaction(SIGTERM, &newAction, NULL);
#else
	signal(SIGINT, quitAll);
#ifdef SIGQUIT
	signal(SIGQUIT, quitAll);
#endif
	signal(SIGTERM, quitAll);
#endif

	// Open the daemon index in read-write mode 
	bool wasObsoleteFormat = false;
	if (ModuleFactory::openOrCreateIndex(settings.m_defaultBackend, settings.m_daemonIndexLocation, wasObsoleteFormat, false) == false)
	{
		cerr << "Couldn't open index " << settings.m_daemonIndexLocation << endl;
		return EXIT_FAILURE;
	}
	if (wasObsoleteFormat == true)
	{
		resetHistory = resetLabels = true;
	}

	// Do the same for the history database
	PinotSettings::checkHistoryDatabase();
	string historyDatabase(settings.getHistoryDatabaseName());
	if ((historyDatabase.empty() == true) ||
		(ActionQueue::create(historyDatabase) == false) ||
		(CrawlHistory::create(historyDatabase) == false) ||
		(MetaDataBackup::create(historyDatabase) == false) ||
		(QueryHistory::create(historyDatabase) == false) ||
		(ViewHistory::create(historyDatabase) == false))
	{
		cerr << "Couldn't create history database " << historyDatabase << endl;
		return EXIT_FAILURE;
	}
	else
	{
		ActionQueue actionQueue(historyDatabase, Glib::get_application_name());
		QueryHistory queryHistory(historyDatabase);
		ViewHistory viewHistory(historyDatabase);
		time_t timeNow = time(NULL);
		unsigned int actionsCount = actionQueue.getItemsCount(ActionQueue::INDEX);

		// Don't expire actions left from last time
		actionsCount += actionQueue.getItemsCount(ActionQueue::UNINDEX);
		cout << actionsCount << " actions left" << endl;

		// Expire the rest
		queryHistory.expireItems(timeNow);
		viewHistory.expireItems(timeNow);
	}

	atexit(closeAll);

#ifdef HAVE_LINUX_SCHED_H
	// Set the scheduling policy
	struct sched_param schedParam;
	if (sched_getparam(0, &schedParam) == -1)
	{
		cerr << "Couldn't get current scheduling policy" << endl;
	}
	else if (sched_setscheduler(0, SCHED_IDLE, &schedParam) == -1)
	{
		cerr << "Couldn't set scheduling policy" << endl;
	}
#else
	// Change the daemon's priority
	if (setpriority(PRIO_PROCESS, 0, priority) == -1)
	{
		cerr << "Couldn't set scheduling priority to " << priority << endl;
	}
#endif

	GError *pError = NULL;
	DBusGConnection *pBus = dbus_g_bus_get(DBUS_BUS_SESSION, &pError);
	if (pBus == NULL)
	{
		if (pError != NULL)
		{
			cerr << "Couldn't open bus connection: " << pError->message << endl;
			if (pError->message != NULL)
			{
				cerr << "Error is " << pError->message << endl;
			}
			g_error_free(pError);
		}

		return EXIT_FAILURE;
	}

	DBusConnection *pConnection = dbus_g_connection_get_connection(pBus);
	if (pConnection == NULL)
	{
		cerr << "Couldn't get connection" << endl;
		return EXIT_FAILURE;
	}

	DBusError error;
	DaemonState server;
	IndexInterface *pIndex = NULL;

	dbus_error_init(&error);
	dbus_connection_set_exit_on_disconnect(pConnection, FALSE);
	dbus_connection_setup_with_g_main(pConnection, NULL);

	if (dbus_connection_register_object_path(pConnection, g_pinotDBusObjectPath,
		&g_callVTable, &server) == TRUE)
	{
		// Request to be identified by this name
		// FIXME: flags are currently broken ?
		dbus_bus_request_name(pConnection, g_pinotDBusService, 0, &error);
		if (dbus_error_is_set(&error) == FALSE)
		{
			// See power management signals
			dbus_bus_add_match(pConnection,
				"type='signal',interface='org.freedesktop.PowerManagement'", &error);
			dbus_bus_add_match(pConnection,
				"type='signal',interface='org.gnome.PowerManager'", &error);

			dbus_connection_add_filter(pConnection,
				(DBusHandleMessageFunction)filterHandler, &server, NULL);
		}
		else
		{
			cerr << "Couldn't obtain name " << g_pinotDBusService << endl;
			if (error.message != NULL)
			{
				cerr << "Error is " << error.message << endl;
			}
		}

		try
		{
			set<string> labels;
			bool gotLabels = false;
			bool onBattery = false;

			pIndex = settings.getIndex(settings.m_daemonIndexLocation);
			if (pIndex != NULL)
			{
				string indexVersion(pIndex->getMetadata("version"));

				gotLabels = pIndex->getLabels(labels);

				// What version is the index at ?
				if (indexVersion.empty() == true)
				{
					indexVersion = "0.0";
				}
				if (ignoreVersion == true)
				{
					// Better reset labels, they may have been lost too
					resetLabels = true;
				}
				else if (indexVersion < PINOT_INDEX_MIN_VERSION)
				{
					cout << "Upgrading index from version " << indexVersion << " to " << VERSION << endl;

					reindex = true;
				}
				if (reindex == true)
				{
					// Reset the index so that all documents are reindexed
					pIndex->reset();

					cout << "Reset index" << endl;

					resetHistory = resetLabels = true;
				}

				pIndex->setMetadata("version", VERSION);
				pIndex->setMetadata("dbus-status", "Running");
			}

			if (resetHistory == true)
			{
				CrawlHistory history(historyDatabase);
				map<unsigned int, string> sources;

				// Reset the history
				history.getSources(sources);
				for (std::map<unsigned int, string>::iterator sourceIter = sources.begin();
					sourceIter != sources.end(); ++sourceIter)
				{
					history.deleteItems(sourceIter->first);
					history.deleteSource(sourceIter->first);
				}

				cout << "Reset crawler history" << endl;
			}

			if ((resetLabels == true) &&
				(pIndex != NULL))
			{
				// Re-apply the labels list
				if (gotLabels == false)
				{
					// If this is an upgrade from a version < 0.80, the labels list
					// needs to be pulled from the configuration file
					pIndex->setLabels(settings.m_labels, true);

					cout << "Set labels as per the configuration file" << endl;
				}
				else
				{
					pIndex->setLabels(labels, true);
				}
			}

			// Connect to the quit signal
			server.getQuitSignal().connect(sigc::ptr_fun(&quitAll));

			// Connect to threads' finished signal
			server.connect();

			// Try and get the battery state
			gboolean result = FALSE;
			if ((getBatteryState(pBus, "org.freedesktop.PowerManagement",
				"/org/freedesktop/PowerManagement", "GetOnBattery", result) == true) ||
				(getBatteryState(pBus, "org.freedesktop.PowerManagement",
				"/org/freedesktop/PowerManagement", "GetBatteryState", result) == true))
			{
				if (result == TRUE)
				{
					onBattery = true;
				}
			}
			else if (getBatteryState(pBus, "org.gnome.PowerManager",
				"/org/gnome/PowerManager", "GetOnAc", result) == true)
			{
				if (result == FALSE)
				{
					onBattery = true;
				}
			}
			if (onBattery == true)
			{
				// We are on battery
				server.set_flag(DaemonState::ON_BATTERY);
				server.stop_crawling();

				cout << "System is on battery" << endl;
			}

			server.start(fullScan, reindex);

			// Run the main loop
			g_refMainLoop->run();

		}
		catch (const Glib::Exception &e)
		{
			cerr << e.what() << endl;
			return EXIT_FAILURE;
		}
		catch (const char *pMsg)
		{
			cerr << pMsg << endl;
			return EXIT_FAILURE;
		}
		catch (...)
		{
			cerr << "Unknown exception" << endl;
			return EXIT_FAILURE;
		}
	}
	else
	{
		cerr << "Couldn't register object path" << endl;
	}
	dbus_error_free(&error);

	if (pIndex != NULL)
	{
		if (server.is_flag_set(DaemonState::DISCONNECTED) == true)
		{
			pIndex->setMetadata("dbus-status", "Disconnected");
		}
		else if (server.is_flag_set(DaemonState::STOPPED) == true)
		{
			pIndex->setMetadata("dbus-status", "Stopped");
		}
		else
		{
			pIndex->setMetadata("dbus-status", "");
		}
		delete pIndex;
	}

	// Stop everything
	server.disconnect();
	server.stop_threads();

	return EXIT_SUCCESS;
}
