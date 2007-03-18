/*
 *  Copyright 2005,2006 Fabrice Colin
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

#include <string>
#include <fstream>
#include <map>
#include <set>
#include <iostream>
#include <sigc++/class_slot.h>
#include <glibmm/convert.h>
#include <gtkmm/stock.h>

#include "config.h"
#include "NLS.h"
#include "Url.h"
#include "CrawlHistory.h"
#include "ViewHistory.h"
#include "PinotSettings.h"
#include "PinotUtils.h"
#include "statisticsDialog.hh"

using namespace std;
using namespace Glib;
using namespace Gtk;

statisticsDialog::statisticsDialog() :
	statisticsDialog_glade(),
	m_hasErrors(false),
	m_lastErrorDate(0)
{
	// Associate the columns model to the engines tree
	m_refStore = TreeStore::create(m_statsColumns);
	statisticsTreeview->set_model(m_refStore);

	TreeViewColumn *pColumn = create_column(_("Status"), m_statsColumns.m_name, true, true, m_statsColumns.m_name);
	if (pColumn != NULL)
	{
		statisticsTreeview->append_column(*manage(pColumn));
	}

	// Populate
	populate();
	// ...and update regularly
	m_idleConnection = Glib::signal_timeout().connect(SigC::slot(*this,
		&statisticsDialog::on_activity_timeout), 5000);
}

statisticsDialog::~statisticsDialog()
{
	m_idleConnection.disconnect();
}

void statisticsDialog::populate(void)
{
	TreeModel::iterator folderIter = m_refStore->append();
	TreeModel::Row row = *folderIter;
	row[m_statsColumns.m_name] = _("Indexes");
	statisticsTreeview->get_selection()->select(folderIter);

	TreeModel::iterator statIter = m_refStore->append(folderIter->children());
	row = *statIter;
	row[m_statsColumns.m_name] = _("My Web Pages");
	m_myWebPagesIter = m_refStore->append(statIter->children());

	statIter = m_refStore->append(folderIter->children());
	row = *statIter;
	row[m_statsColumns.m_name] = _("My Documents");
	m_myDocumentsIter = m_refStore->append(statIter->children());

	folderIter = m_refStore->append();
	row = *folderIter;
	row[m_statsColumns.m_name] = _("History");
	m_viewStatIter = m_refStore->append(folderIter->children());

	m_daemonIter = m_refStore->append();
	row = *m_daemonIter;
	row[m_statsColumns.m_name] = _("Daemon");
	m_daemonStatIter = m_refStore->append(m_daemonIter->children());

	m_crawledStatIter = m_refStore->append(folderIter->children());

	// Expand everything
	statisticsTreeview->expand_all();

	Adjustment *pAdjustement = statisticsScrolledwindow->get_hadjustment();
#ifdef DEBUG
	cout << "statisticsDialog: " << pAdjustement->get_value() << " "
		<< pAdjustement->get_lower() << " " << pAdjustement->get_upper() << endl;
#endif
	pAdjustement->set_value(pAdjustement->get_upper());
	statisticsScrolledwindow->set_hadjustment(pAdjustement);
	resize((int )pAdjustement->get_upper() * 2, get_height());

	on_activity_timeout();
}

bool statisticsDialog::on_activity_timeout(void)
{
	CrawlHistory crawlerHistory(PinotSettings::getInstance().m_historyDatabase);
	ViewHistory viewHistory(PinotSettings::getInstance().m_historyDatabase);
	TreeModel::Row row;
	std::map<unsigned int, string> sources;
	char countStr[64];

	row = *m_myWebPagesIter;
	IndexInterface *pIndex = PinotSettings::getInstance().getIndex(PinotSettings::getInstance().m_docsIndexLocation);
	if ((pIndex != NULL) &&
		(pIndex->isGood() == true))
	{
		unsigned int docsCount = pIndex->getDocumentsCount();

		snprintf(countStr, 64, "%u", docsCount);
		row[m_statsColumns.m_name] = ustring(countStr) + " " + _("documents");
	}
	else
	{
		row[m_statsColumns.m_name] = _("Unknown error");
	}

	row = *m_myDocumentsIter;
	pIndex = PinotSettings::getInstance().getIndex(PinotSettings::getInstance().m_daemonIndexLocation);
	if ((pIndex != NULL) &&
		(pIndex->isGood() == true))
	{
		unsigned int docsCount = pIndex->getDocumentsCount();

		snprintf(countStr, 64, "%u", docsCount);
		row[m_statsColumns.m_name] = ustring(countStr) + " " + _("documents");
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
	row = *m_daemonStatIter;
	if (daemonPID > 0)
	{
		// FIXME: check whether it's actually running !
		row[m_statsColumns.m_name] = ustring(_("Running under PID")) + " " + countStr;
	}
	else
	{
		row[m_statsColumns.m_name] = ustring(_("Currently not running"));
	}

	// Show crawler statistics
	unsigned int crawledFilesCount = crawlerHistory.getItemsCount(CrawlHistory::CRAWLED);
	snprintf(countStr, 64, "%u", crawledFilesCount);
	row = *m_crawledStatIter;
	row[m_statsColumns.m_name] = ustring(_("Crawled")) + " " + countStr + " " + _("files");

	// Show errors
	crawlerHistory.getSources(sources);
	for (std::map<unsigned int, string>::iterator sourceIter = sources.begin();
		sourceIter != sources.end(); ++sourceIter)
	{
		set<string> errors;

		// Did any error occur on this source ?
		unsigned int errorCount = crawlerHistory.getSourceItems(sourceIter->first,
			CrawlHistory::ERROR, errors, m_lastErrorDate);
		if ((errorCount > 0) &&
			(errors.empty() == false))
		{
			Url urlObj(sourceIter->second);
			ustring sourceName(urlObj.getLocation());

			// Add an errors row
			if (m_hasErrors == false)
			{
				m_errorsIter  = m_refStore->append(m_daemonIter->children());
				row = *m_errorsIter;
				row[m_statsColumns.m_name] = _("Errors");

				// Don't expand it
				TreeModel::Path errPath = m_refStore->get_path(m_errorsIter);
				statisticsTreeview->collapse_row(errPath);

				m_hasErrors = true;
			}

			// Add the source
			TreeModel::iterator srcIter = m_refStore->append(m_errorsIter->children());
			row = *srcIter;
			if (urlObj.getFile().empty() == false)
			{
				sourceName += "/";
				sourceName += urlObj.getFile();
			}
			row[m_statsColumns.m_name] = sourceName;

			// List them
			for (set<string>::const_iterator errorIter = errors.begin();
				errorIter != errors.end(); ++errorIter)
			{
				TreeModel::iterator errIter = m_refStore->append(srcIter->children());
				row = *errIter;
				row[m_statsColumns.m_name] = (*errorIter);
			}
		}
	}
	m_lastErrorDate = time(NULL);

	return true;
}

