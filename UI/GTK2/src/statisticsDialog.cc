/*
 *  Copyright 2005-2008 Fabrice Colin
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

#include <string>
#include <fstream>
#include <map>
#include <set>
#include <iostream>
#include <glibmm/convert.h>
#include <gtkmm/stock.h>

#include "config.h"
#include "NLS.h"
#include "Url.h"
#include "CrawlHistory.h"
#include "ViewHistory.h"
#include "DBusIndex.h"
#include "ModuleFactory.h"
#include "PinotSettings.h"
#include "PinotUtils.h"
#include "WorkerThreads.h"
#include "statisticsDialog.hh"

using namespace std;
using namespace Glib;
using namespace Gtk;

statisticsDialog::statisticsDialog() :
	statisticsDialog_glade(),
	m_hasErrors(false),
	m_getStats(true)
{
	// Associate the columns model to the engines tree
	m_refStore = TreeStore::create(m_statsColumns);
	statisticsTreeview->set_model(m_refStore);

	TreeViewColumn *pColumn = create_column(_("Status"), m_statsColumns.m_name, true, true, m_statsColumns.m_name);
	if (pColumn != NULL)
	{
		statisticsTreeview->append_column(*manage(pColumn));
	}

	// Resize the window
	resize((int )(get_width() * 2), (int )(get_height() * 1.5));

	// Populate
	populate();
	// ...and update regularly
	m_idleConnection = Glib::signal_timeout().connect(sigc::mem_fun(*this,
		&statisticsDialog::on_activity_timeout), 10000);
}

statisticsDialog::~statisticsDialog()
{
	m_idleConnection.disconnect();
}

void statisticsDialog::populate(void)
{
	TreeModel::iterator folderIter = m_refStore->append();
	TreeModel::Row row = *folderIter;
	std::map<ModuleProperties, bool> engines;

	// Indexes
	statisticsTreeview->get_selection()->select(folderIter);
	row[m_statsColumns.m_name] = _("Indexes");
	TreeModel::iterator statIter = m_refStore->append(folderIter->children());
	row = *statIter;
	row[m_statsColumns.m_name] = _("My Web Pages");
	m_myWebPagesIter = m_refStore->append(statIter->children());
	statIter = m_refStore->append(folderIter->children());
	row = *statIter;
	row[m_statsColumns.m_name] = _("My Documents");
	m_myDocumentsIter = m_refStore->append(statIter->children());

	// Search engines
	TreeModel::iterator enginesIter = m_refStore->append();
	row = *enginesIter;
	row[m_statsColumns.m_name] = _("Search Engines");
	ModuleFactory::getSupportedEngines(engines);
	for (std::map<ModuleProperties, bool>::const_iterator engineIter = engines.begin();
		engineIter != engines.end(); ++engineIter)
	{
		TreeModel::iterator statIter = m_refStore->append(enginesIter->children());
		row = *statIter;
		row[m_statsColumns.m_name] = engineIter->first.m_name;
	}

	// History
	folderIter = m_refStore->append();
	row = *folderIter;
	row[m_statsColumns.m_name] = _("History");
	m_viewStatIter = m_refStore->append(folderIter->children());
	m_crawledStatIter = m_refStore->append(folderIter->children());

	// Daemon
	m_daemonIter = m_refStore->append();
	row = *m_daemonIter;
	row[m_statsColumns.m_name] = _("Daemon");
	m_daemonProcIter = m_refStore->append(m_daemonIter->children());
	m_diskSpaceIter = m_refStore->append(m_daemonIter->children());
	m_batteryIter = m_refStore->append(m_daemonIter->children());
	m_crawlIter = m_refStore->append(m_daemonIter->children());

	// Expand everything
	statisticsTreeview->expand_all();
	TreeModel::Path enginesPath = m_refStore->get_path(enginesIter);
	statisticsTreeview->collapse_row(enginesPath);

	on_activity_timeout();
}

bool statisticsDialog::on_activity_timeout(void)
{
	CrawlHistory crawlHistory(PinotSettings::getInstance().getHistoryDatabaseName(true));
	ViewHistory viewHistory(PinotSettings::getInstance().getHistoryDatabaseName());
	TreeModel::Row row;
	std::map<unsigned int, string> sources;
	ustring yes(_("Yes")), no(_("No"));
	string daemonDBusStatus;
	char countStr[64];
	bool lowDiskSpace = false, onBattery = false, crawling = false;

	row = *m_myWebPagesIter;
	IndexInterface *pIndex = PinotSettings::getInstance().getIndex(PinotSettings::getInstance().m_docsIndexLocation);
	if (pIndex != NULL)
	{
		unsigned int docsCount = pIndex->getDocumentsCount();

		snprintf(countStr, 64, "%u", docsCount);
		row[m_statsColumns.m_name] = ustring(countStr) + " " + _("documents");

		delete pIndex;
	}
	else
	{
		row[m_statsColumns.m_name] = _("Unknown error");
	}

	row = *m_myDocumentsIter;
	pIndex = PinotSettings::getInstance().getIndex(PinotSettings::getInstance().m_daemonIndexLocation);
	if (pIndex != NULL)
	{
		unsigned int docsCount = pIndex->getDocumentsCount();

		snprintf(countStr, 64, "%u", docsCount);
		row[m_statsColumns.m_name] = ustring(countStr) + " " + _("documents");

		daemonDBusStatus = pIndex->getMetadata("dbus-status");

		delete pIndex;
	}
	else
	{
		row[m_statsColumns.m_name] = _("Unknown error");
	}

	// Show view statistics
	unsigned int viewCount = viewHistory.getItemsCount();
	snprintf(countStr, 64, "%u", viewCount);
	row = *m_viewStatIter;
	row[m_statsColumns.m_name] = ustring(_("Viewed")) + " " + countStr + " " + _("results");

	// Show crawler statistics
	unsigned int crawledFilesCount = crawlHistory.getItemsCount(CrawlHistory::CRAWLED);
	snprintf(countStr, 64, "%u", crawledFilesCount);
	row = *m_crawledStatIter;
	row[m_statsColumns.m_name] = ustring(_("Crawled")) + " " + countStr + " " + _("files");

	// Is the daemon still running ?
	string pidFileName(PinotSettings::getInstance().getConfigurationDirectory() + "/pinot-dbus-daemon.pid");
	pid_t daemonPID = 0;
	ifstream pidFile;
	pidFile.open(pidFileName.c_str());
	if (pidFile.is_open() == true)
	{
		pidFile >> daemonPID;
		pidFile.close();
	}
	snprintf(countStr, 64, "%u", daemonPID);

	row = *m_daemonProcIter;
	if (daemonPID > 0)
	{
		// FIXME: check whether it's actually running !
		row[m_statsColumns.m_name] = ustring(_("Running under PID")) + " " + countStr;

		if (m_getStats == true)
		{
			unsigned int crawledCount = 0, docsCount = 0;

			if (DBusIndex::getStatistics(crawledCount, docsCount, lowDiskSpace, onBattery, crawling) == false)
			{
#ifdef DEBUG
				cout << "statisticsDialog::on_activity_timeout: failed to get statistics" << endl;
#endif

				// Don't try again
				m_getStats = false;
			}
		}
	}
	else
	{
		if (daemonDBusStatus == "Disconnected")
		{
			row[m_statsColumns.m_name] = _("Disconnected from D-Bus");
		}
		else if (daemonDBusStatus == "Stopped")
		{
			row[m_statsColumns.m_name] = _("Stopped");
		}
		else
		{
			row[m_statsColumns.m_name] = _("Currently not running");
		}
	}

	// Show status
	row = *m_diskSpaceIter;
	row[m_statsColumns.m_name] = ustring(_("Low disk space")) + ": " + (lowDiskSpace == true ? yes : no);
	row = *m_batteryIter;
	row[m_statsColumns.m_name] = ustring(_("On battery")) + ": " + (onBattery == true ? yes : no);
	row = *m_crawlIter;
	row[m_statsColumns.m_name] = ustring(_("Crawling")) + ": "+ (crawling == true ? yes : no);

	// Show errors
	crawlHistory.getSources(sources);
	for (std::map<unsigned int, string>::iterator sourceIter = sources.begin();
		sourceIter != sources.end(); ++sourceIter)
	{
		unsigned int sourceNum(sourceIter->first);
		set<string> errors;
		time_t latestErrorDate = 0;

		std::map<unsigned int, time_t>::const_iterator dateIter = m_latestErrorDates.find(sourceNum);
		if (dateIter != m_latestErrorDates.end())
		{
			latestErrorDate = dateIter->second;
		}

		// Did any error occur on this source ?
		unsigned int errorCount = crawlHistory.getSourceItems(sourceNum,
			CrawlHistory::ERROR, errors, latestErrorDate);
		if ((errorCount > 0) &&
			(errors.empty() == false))
		{
			// Add an errors row
			if (m_hasErrors == false)
			{
				m_errorsTopIter = m_refStore->append(m_daemonIter->children());
				row = *m_errorsTopIter;
				row[m_statsColumns.m_name] = _("Errors");

				m_hasErrors = true;
			}

			// List them
			for (set<string>::const_iterator errorIter = errors.begin();
				errorIter != errors.end(); ++errorIter)
			{
				string locationWithError(*errorIter);
				TreeModel::iterator errIter;
				time_t errorDate;
				int errorNum = crawlHistory.getErrorDetails(locationWithError, errorDate);

				if (errorDate > latestErrorDate)
				{
					latestErrorDate = errorDate;
				}

				// Find or create the iterator for this particular kind of error
				std::map<int, TreeModel::iterator>::const_iterator errorTreeIter = m_errorsIters.find(errorNum);
				if (errorTreeIter == m_errorsIters.end())
				{
					string errorText(WorkerThread::errorToString(errorNum));

					errIter = m_refStore->append(m_errorsTopIter->children());
					row = *errIter;
					row[m_statsColumns.m_name] = errorText;

					m_errorsIters.insert(pair<int, TreeModel::iterator>(errorNum, errIter));
				}
				else
				{
					errIter = errorTreeIter->second;
				}

				// Display the location itself
				TreeModel::iterator locIter = m_refStore->append(errIter->children());
				row = *locIter;
				row[m_statsColumns.m_name] = locationWithError;
			}

			// Expand errors
			TreeModel::Path errPath = m_refStore->get_path(m_errorsTopIter);
			statisticsTreeview->expand_to_path(errPath);
		}

		// The next check will ignore errors older than this
		m_latestErrorDates[sourceNum] = latestErrorDate;
	}

	return true;
}

