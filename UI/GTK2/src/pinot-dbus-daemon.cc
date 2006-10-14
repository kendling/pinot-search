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

#include "TokenizerFactory.h"
#include "Languages.h"
#include "MIMEScanner.h"
#include "XapianDatabase.h"
#include "XapianDatabaseFactory.h"
#include "HtmlTokenizer.h"
#include "CrawlHistory.h"
#include "QueryHistory.h"
#include "ViewHistory.h"
#include "DownloaderInterface.h"
#include "XapianIndex.h"
#include "XapianEngine.h"
#include "config.h"
#include "NLS.h"
#include "DaemonState.h"
#include "PinotSettings.h"

using namespace std;

static ofstream g_outputFile;
static streambuf *g_coutBuf = NULL;
static streambuf *g_cerrBuf = NULL;
static struct option g_longOptions[] = {
	{"help", 0, 0, 'h'},
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

	// Close all indexes we may have opened
	XapianDatabaseFactory::closeAll();

	// Close the tokenizer libraries
	TokenizerFactory::unloadTokenizers();

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

	DownloaderInterface::shutdown();
	HtmlTokenizer::shutdown();
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

static DBusMessage *newDBusReply(DBusMessage *pMessage)
{
	if (pMessage == NULL)
	{
		return NULL;
	}

	DBusMessage *pReply = dbus_message_new_method_return(pMessage);
	if (pReply != NULL)
	{
		return pReply;
	}

	return NULL;
}

static DBusHandlerResult filterHandler(DBusConnection *pConnection, DBusMessage *pMessage, void *pData)
{
#ifdef DEBUG
	cout << "filterHandler: called" << endl;
#endif
	// Are we about to be disconnected ?
	if (dbus_message_is_signal(pMessage, DBUS_INTERFACE_LOCAL, "Disconnected") == TRUE)
	{
#ifdef DEBUG
		cout << "filterHandler: received Disconnected" << endl;
#endif
		quitAll(0);
	}
	else if (dbus_message_is_signal(pMessage, DBUS_INTERFACE_DBUS, "NameOwnerChanged") == TRUE)
	{
#ifdef DEBUG
		cout << "filterHandler: received NameOwnerChanged" << endl;
#endif
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
	XapianIndex index(PinotSettings::getInstance().m_daemonIndexLocation);
	DaemonState *pServer = NULL;
	DBusMessage *pReply = NULL;
	DBusError error;
	const char *pSender = dbus_message_get_sender(pMessage);
	bool processedMessage = true, flushIndex = false, quitLoop = false;

	if (pData != NULL)
	{
		pServer = (DaemonState *)pData;
	}

	dbus_error_init(&error);

#ifdef DEBUG
	if (pSender != NULL)
	{
		cout << "messageHandler: called by " << pSender << endl;
	}
	else
	{
		cout << "messageHandler: called by unknown sender" << endl;
	}
#endif

	if (dbus_message_is_method_call(pMessage, g_pinotDBusService, "DeleteLabel") == TRUE)
	{
		char *pLabel = NULL;

		if (dbus_message_get_args(pMessage, &error,
			DBUS_TYPE_STRING, &pLabel,
			DBUS_TYPE_INVALID) == TRUE)
		{
#ifdef DEBUG
			cout << "messageHandler: received DeleteLabel " << pLabel << endl;
#endif
			// Delete the label
			flushIndex = index.deleteLabel(pLabel);

			// Prepare the reply
			pReply = newDBusReply(pMessage);
			if (pReply != NULL)
			{
				dbus_message_append_args(pReply,
					DBUS_TYPE_STRING, &pLabel,
					DBUS_TYPE_INVALID);
			}
		}
	}
	else if (dbus_message_is_method_call(pMessage, g_pinotDBusService, "GetDocumentInfo") == TRUE)
	{
		unsigned int docId = 0;

		if (dbus_message_get_args(pMessage, &error,
			DBUS_TYPE_UINT32, &docId,
			DBUS_TYPE_INVALID) == TRUE)
		{
			DocumentInfo docInfo;

#ifdef DEBUG
			cout << "messageHandler: received GetDocumentInfo " << docId << endl;
#endif
			if (index.getDocumentInfo(docId, docInfo) == true)
			{
				// Prepare the reply
				pReply = newDBusReply(pMessage);
				if (pReply != NULL)
				{
					string language(Languages::toEnglish(docInfo.getLanguage()));
					const char *pTitle = docInfo.getTitle().c_str();
					const char *pLocation = docInfo.getLocation().c_str();
					const char *pType = docInfo.getType().c_str();
					const char *pLanguage = language.c_str();

					dbus_message_append_args(pReply,
						DBUS_TYPE_STRING, &pTitle,
						DBUS_TYPE_STRING, &pLocation,
						DBUS_TYPE_STRING, &pType,
						DBUS_TYPE_STRING, &pLanguage,
						DBUS_TYPE_INVALID);
				}
			}
			else
			{
				pReply = dbus_message_new_error(pMessage,
					"de.berlios.Pinot.GetDocumentInfo",
					"Unknown document");
			}
		}
	}
	else if (dbus_message_is_method_call(pMessage, g_pinotDBusService, "GetDocumentLabels") == TRUE)
	{
		unsigned int docId = 0;

		if (dbus_message_get_args(pMessage, &error,
			DBUS_TYPE_UINT32, &docId,
			DBUS_TYPE_INVALID) == TRUE)
		{
			set<string> labels;

#ifdef DEBUG
			cout << "messageHandler: received GetDocumentLabels " << docId << endl;
#endif
			if (index.getDocumentLabels(docId, labels) == true)
			{
				dbus_uint32_t labelsCount = labels.size();
				GPtrArray *pLabels = g_ptr_array_new();

				for (set<string>::const_iterator labelIter = labels.begin();
					labelIter != labels.end(); ++labelIter)
				{
					string labelName(*labelIter);

					g_ptr_array_add(pLabels, const_cast<char*>(labelName.c_str()));
#ifdef DEBUG
					cout << "messageHandler: adding label " << pLabels->len << " " << labelName << endl;
#endif
				}

				// Prepare the reply
				pReply = newDBusReply(pMessage);
				if (pReply != NULL)
				{
					dbus_message_append_args(pReply,
						DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &pLabels->pdata, pLabels->len,
						DBUS_TYPE_INVALID);

					// Send the reply here
					dbus_connection_send(pConnection, pReply, NULL);
					dbus_message_unref(pReply);

					pReply = NULL;
				}

				// Free the array
				g_ptr_array_free(pLabels, TRUE);
			}
			else
			{
				pReply = dbus_message_new_error(pMessage,
					"de.berlios.Pinot.GetDocumentLabels",
					" failed");
			}
		}
	}
	else if (dbus_message_is_method_call(pMessage, g_pinotDBusService, "GetStatistics") == TRUE)
	{
		CrawlHistory history(PinotSettings::getInstance().m_historyDatabase);
		unsigned int crawledFilesCount = history.getItemsCount();
		unsigned int docsCount = index.getDocumentsCount();
		unsigned int docId = 0;

#ifdef DEBUG
		cout << "messageHandler: received GetStatistics" << endl;
#endif
		// Prepare the reply
		pReply = newDBusReply(pMessage);
		if (pReply != NULL)
		{
			dbus_message_append_args(pReply,
				DBUS_TYPE_UINT32, &crawledFilesCount,
				DBUS_TYPE_UINT32, &docsCount,
				DBUS_TYPE_INVALID);
		}
	}
	else if (dbus_message_is_method_call(pMessage, g_pinotDBusService, "RenameLabel") == TRUE)
	{
		char *pOldLabel = NULL;
		char *pNewLabel = NULL;

		if (dbus_message_get_args(pMessage, &error,
			DBUS_TYPE_STRING, &pOldLabel,
			DBUS_TYPE_STRING, &pNewLabel,
			DBUS_TYPE_INVALID) == TRUE)
		{
#ifdef DEBUG
			cout << "messageHandler: received RenameLabel " << pOldLabel << ", " << pNewLabel << endl;
#endif
			// Rename the label
			flushIndex = index.renameLabel(pOldLabel, pNewLabel);

			// Prepare the reply
			pReply = newDBusReply(pMessage);
			if (pReply != NULL)
			{
				dbus_message_append_args(pReply,
					DBUS_TYPE_STRING, &pNewLabel,
					DBUS_TYPE_INVALID);
			}
		}
	}
	else if (dbus_message_is_method_call(pMessage, g_pinotDBusService, "SetDocumentInfo") == TRUE)
	{
		char *pTitle = NULL;
		char *pLocation = NULL;
		char *pType = NULL;
		char *pLanguage = NULL;
		unsigned int docId = 0;

		if (dbus_message_get_args(pMessage, &error,
			DBUS_TYPE_UINT32, &docId,
			DBUS_TYPE_STRING, &pTitle,
			DBUS_TYPE_STRING, &pLocation,
			DBUS_TYPE_STRING, &pType,
			DBUS_TYPE_STRING, &pLanguage,
			DBUS_TYPE_INVALID) == TRUE)
		{
			DocumentInfo docInfo(pTitle, pLocation, pType,
				((pLanguage != NULL) ? Languages::toLocale(pLanguage) : ""));

#ifdef DEBUG
			cout << "messageHandler: received SetDocumentInfo " << docId << ", " << pTitle
				<< ", " << pLocation << ", " << pType << ", " << pLanguage << endl;
#endif

			// Update the document info
			flushIndex = index.updateDocumentInfo(docId, docInfo);

			// Prepare the reply
			pReply = newDBusReply(pMessage);
			if (pReply != NULL)
			{
				dbus_message_append_args(pReply,
					DBUS_TYPE_UINT32, &docId,
					DBUS_TYPE_INVALID);
			}
		}
	}
	else if (dbus_message_is_method_call(pMessage, g_pinotDBusService, "SetDocumentLabels") == TRUE)
	{
		char **ppLabels = NULL;
		dbus_uint32_t labelsCount = 0;
		unsigned int docId = 0;
		gboolean resetLabels = TRUE;

		if (dbus_message_get_args(pMessage, &error,
			DBUS_TYPE_UINT32, &docId,
			DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &ppLabels, &labelsCount,
			DBUS_TYPE_BOOLEAN, &resetLabels,
			DBUS_TYPE_INVALID) == TRUE)
		{
			set<string> labels;

			for (dbus_uint32_t labelIndex = 0; labelIndex < labelsCount; ++labelIndex)
			{
				if (ppLabels[labelIndex] == NULL)
				{
					break;
				}
				labels.insert(ppLabels[labelIndex]);
			}
#ifdef DEBUG
			cout << "messageHandler: received SetDocumentLabels " << docId << ", " << resetLabels
				<< " with " << labelsCount << " labels" << endl;
#endif
			// Set labels
			flushIndex = index.setDocumentLabels(docId, labels, ((resetLabels == TRUE) ? true : false));

			// Free container types
			g_strfreev(ppLabels);

			// Prepare the reply
			pReply = newDBusReply(pMessage);
			if (pReply != NULL)
			{
				dbus_message_append_args(pReply,
					DBUS_TYPE_UINT32, &docId,
					DBUS_TYPE_INVALID);
			}
		}
	}
	else if (dbus_message_is_method_call(pMessage, g_pinotDBusService, "SimpleQuery") == TRUE)
	{
		char *pSearchText = NULL;
		dbus_uint32_t maxHits = 0;
		unsigned int docId = 0;

		if (dbus_message_get_args(pMessage, &error,
			DBUS_TYPE_STRING, &pSearchText,
			DBUS_TYPE_UINT32, &maxHits,
			DBUS_TYPE_INVALID) == TRUE)
		{
			XapianEngine engine(PinotSettings::getInstance().m_daemonIndexLocation);
			bool replyWithError = true;

#ifdef DEBUG
			cout << "messageHandler: received SimpleQuery " << pSearchText << ", " << maxHits << endl;
#endif
			if (pSearchText != NULL)
			{
				QueryProperties queryProps("DBUS", pSearchText);

				// Run the query
				engine.setMaxResultsCount(maxHits);
				if (engine.runQuery(queryProps) == true)
				{
					const vector<Result> &resultsList = engine.getResults();
					vector<string> docIds;
					GPtrArray *pDocIds = g_ptr_array_new();

					for (vector<Result>::const_iterator resultIter = resultsList.begin();
						resultIter != resultsList.end(); ++resultIter)
					{
						// We only need the document ID
						unsigned int docId = index.hasDocument(resultIter->getLocation());
						if (docId > 0)
						{
							char docIdStr[64];
							snprintf(docIdStr, 64, "%u", docId);
							docIds.push_back(docIdStr);
						}
					}

					for (vector<string>::const_iterator docIter = docIds.begin();
						docIter != docIds.end(); ++docIter)
					{
#ifdef DEBUG
						cout << "messageHandler: adding result " << pDocIds->len << " " << *docIter << endl;
#endif
						g_ptr_array_add(pDocIds, const_cast<char*>(docIter->c_str()));
					}

					// Prepare the reply
					pReply = newDBusReply(pMessage);
					if (pReply != NULL)
					{
						dbus_message_append_args(pReply,
							DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &pDocIds->pdata, pDocIds->len,
							DBUS_TYPE_INVALID);

						// Send the reply here
						dbus_connection_send(pConnection, pReply, NULL);
						dbus_message_unref(pReply);

						pReply = NULL;
						replyWithError = false;
					}

					// Free the array
					g_ptr_array_free(pDocIds, TRUE);
				}
			}

			if (replyWithError == true)
			{
				pReply = dbus_message_new_error(pMessage,
					"de.berlios.Pinot.SimpleQuery",
					"Query failed");
			}
		}
	}
	else if (dbus_message_is_method_call(pMessage, g_pinotDBusService, "Stop") == TRUE)
	{
		if (dbus_message_get_args(pMessage, &error,
			DBUS_TYPE_INVALID) == TRUE)
		{
			int exitStatus = EXIT_SUCCESS;

#ifdef DEBUG
			cout << "messageHandler: received Stop" << endl;
#endif
			// Prepare the reply
			pReply = newDBusReply(pMessage);
			if (pReply != NULL)
			{
				dbus_message_append_args(pReply,
					DBUS_TYPE_INT32, &exitStatus,
					DBUS_TYPE_INVALID);
			}

			quitLoop = true;
		}
	}
	else if (dbus_message_is_method_call(pMessage, g_pinotDBusService, "UpdateDocument") == TRUE)
	{
		unsigned int docId = 0;

		if (dbus_message_get_args(pMessage, &error,
			DBUS_TYPE_UINT32, &docId,
			DBUS_TYPE_INVALID) == TRUE)
		{
			DocumentInfo docInfo;

#ifdef DEBUG
			cout << "messageHandler: received UpdateDocument " << docId << endl;
#endif
			if (index.getDocumentInfo(docId, docInfo) == true)
			{
				// Update document
				pServer->queue_index(docInfo);
			}

			// Prepare the reply
			pReply = newDBusReply(pMessage);
			if (pReply != NULL)
			{
				dbus_message_append_args(pReply,
					DBUS_TYPE_UINT32, &docId,
					DBUS_TYPE_INVALID);
			}
		}
	}
	else
	{
#ifdef DEBUG
		cout << "messageHandler: foreign message for/from " << dbus_message_get_interface(pMessage)
			<< " " << dbus_message_get_member(pMessage) << endl;
#endif
		processedMessage = false;
	}

	// Did an error occur ?
	if (error.message != NULL)
	{
#ifdef DEBUG
		cout << "messageHandler: error occured: " << error.message << endl;
#endif
		// Use the error message as reply
		pReply = dbus_message_new_error(pMessage, error.name, error.message);
	}

	dbus_error_free(&error);

	if (flushIndex == true)
	{
		// Flush now for the sake of the client application
		index.flush();
	}

	// Send a reply ?
	if (pReply != NULL)
	{
		dbus_connection_send(pConnection, pReply, NULL);
		dbus_connection_flush(pConnection);
#ifdef DEBUG
		cout << "messageHandler: sent reply" << endl;
#endif
		dbus_message_unref(pReply);
	}

	if (quitLoop == true)
	{
		quitAll(0);
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

	// This should make Xapian use Flint rather than Quartz
	setenv("XAPIAN_PREFER_FLINT", "1", 1);

	MIMEScanner::initialize();
	HtmlTokenizer::initialize();
	DownloaderInterface::initialize();
	Glib::thread_init();

	// This will create the necessary directories on the first run
	PinotSettings &settings = PinotSettings::getInstance();
	// This is the daemon so disable DBus
	settings.enableDBus(false);

	string confDirectory = PinotSettings::getConfigurationDirectory();
	chdir(confDirectory.c_str());

	// Redirect cout and cerr to a file
	string logFileName = confDirectory;
	logFileName += "/pinot-dbus-daemon.log";
	g_outputFile.open(logFileName.c_str());
	g_coutBuf = cout.rdbuf();
	g_cerrBuf = cerr.rdbuf();
	cout.rdbuf(g_outputFile.rdbuf());
	cerr.rdbuf(g_outputFile.rdbuf());

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

	// Open the daemon index in read-write mode 
	XapianDatabase *pDb = XapianDatabaseFactory::getDatabase(settings.m_daemonIndexLocation, false);
	if ((pDb == NULL) ||
		(pDb->isOpen() == false))
	{
		cerr << "Couldn't open index " << settings.m_daemonIndexLocation << endl;
		return EXIT_FAILURE;
	}

	// Do the same for the history database
	if ((settings.m_historyDatabase.empty() == true) ||
		(CrawlHistory::create(settings.m_historyDatabase) == false) ||
		(QueryHistory::create(settings.m_historyDatabase) == false) ||
		(ViewHistory::create(settings.m_historyDatabase) == false))
	{
		cerr << "Couldn't create history database " << settings.m_historyDatabase << endl;
		return EXIT_FAILURE;
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

	// Initialize the GType and the D-Bus thread system
	g_type_init ();
	dbus_g_thread_init();

	GError *pError = NULL;
	DBusGConnection *pBus = dbus_g_bus_get(DBUS_BUS_SESSION, &pError);
	if (pBus == NULL)
	{
		if (pError != NULL)
		{
			cerr << "Couldn't open bus connection: " << pError->message << endl;
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

	dbus_error_init(&error);
	dbus_connection_set_exit_on_disconnect(pConnection, FALSE);
	dbus_connection_setup_with_g_main(pConnection, NULL);

	if ((dbus_error_is_set(&error) == FALSE) &&
		(dbus_connection_register_object_path(pConnection, g_pinotDBusObjectPath,
			&g_callVTable, &server) == TRUE))
	{
		// Request to be identified by this name
		// FIXME: flags are currently broken ?
		dbus_bus_request_name(pConnection, g_pinotDBusService, 0, &error);

		dbus_connection_add_filter(pConnection, (DBusHandleMessageFunction)filterHandler, &server, NULL);

		// Get the main loop
		g_refMainLoop = Glib::MainLoop::create();

		// Connect to threads' finished signal
		server.connect();

		server.start();

		// Run the main loop
		g_refMainLoop->run();
	}
	else
	{
		cerr << "Couldn't register object path: " << pError->message << endl;
	}

	dbus_error_free(&error);
#ifdef DEBUG
	cout << "Closing connection..." << endl;
#endif
	dbus_connection_close(pConnection);
	dbus_g_connection_unref(pBus);

	return EXIT_SUCCESS;
}
