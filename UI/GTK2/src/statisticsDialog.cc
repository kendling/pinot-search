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

#include <iostream>
#include <string>
#include <map>
#include <set>
#include <sigc++/slot.h>

#include "config.h"
#include "NLS.h"
#include "Url.h"
#include "CrawlHistory.h"
#include "PinotSettings.h"
#include "PinotUtils.h"
#include "statisticsDialog.hh"

using namespace std;
using namespace Glib;
using namespace Gtk;

statisticsDialog::statisticsDialog() :
	statisticsDialog_glade()
{
	// Associate the columns model to the engines tree
	m_refStore = TreeStore::create(m_statsColumns);
	statisticsTreeview->set_model(m_refStore);

	TreeViewColumn *pColumn = create_column(_("Statistics"), m_statsColumns.m_name, true, true, m_statsColumns.m_name);
	if (pColumn != NULL)
	{
		statisticsTreeview->append_column(*manage(pColumn));
	}

	populate();
}

statisticsDialog::~statisticsDialog()
{
}

void statisticsDialog::populate(void)
{
	CrawlHistory history(PinotSettings::getInstance().m_historyDatabase);
	std::map<unsigned int, string> sources;
	char countStr[64];
	bool hasErrors = false;

	TreeModel::iterator folderIter = m_refStore->append();
	TreeModel::Row row = *folderIter;
	row[m_statsColumns.m_name] = _("Indexes");

	TreeModel::iterator statIter = m_refStore->append(folderIter->children());
	row = *statIter;
	row[m_statsColumns.m_name] = _("My Documents");
	IndexInterface *pIndex = PinotSettings::getInstance().getIndex(PinotSettings::getInstance().m_docsIndexLocation);
	if ((pIndex != NULL) &&
		(pIndex->isGood() == true))
	{
		unsigned int docsCount = pIndex->getDocumentsCount();
		snprintf(countStr, 64, "%u", docsCount);
		TreeModel::iterator docsIter = m_refStore->append(statIter->children());
		row = *docsIter;
		row[m_statsColumns.m_name] = ustring(countStr) + " " + _("documents");
	}

	statIter = m_refStore->append(folderIter->children());
	row = *statIter;
	row[m_statsColumns.m_name] = _("My Web Pages");
	pIndex = PinotSettings::getInstance().getIndex(PinotSettings::getInstance().m_daemonIndexLocation);
	if ((pIndex != NULL) &&
		(pIndex->isGood() == true))
	{
		unsigned int docsCount = pIndex->getDocumentsCount();
		snprintf(countStr, 64, "%u", docsCount);
		TreeModel::iterator docsIter = m_refStore->append(statIter->children());
		row = *docsIter;
		row[m_statsColumns.m_name] = ustring(countStr) + " " + _("documents");
	}

	folderIter = m_refStore->append();
	row = *folderIter;
	row[m_statsColumns.m_name] = _("Crawler");

	// Show statistics
	unsigned int crawledFilesCount = history.getItemsCount(CrawlHistory::CRAWLED);
	snprintf(countStr, 64, "%u", crawledFilesCount);
	statIter = m_refStore->append(folderIter->children());
	row = *statIter;
	row[m_statsColumns.m_name] = ustring(countStr) + " " + _("files were crawled");
	unsigned int crawlingFilesCount = history.getItemsCount(CrawlHistory::CRAWLING);
	snprintf(countStr, 64, "%u", crawlingFilesCount);
	statIter = m_refStore->append(folderIter->children());
	row = *statIter;
	row[m_statsColumns.m_name] = ustring(countStr) + " " + _("files are being crawled");

	TreeModel::iterator errIter = m_refStore->append(folderIter->children());
	row = *errIter;
	row[m_statsColumns.m_name] = _("Errors");

	history.getSources(sources);
	for (std::map<unsigned int, string>::iterator sourceIter = sources.begin();
		sourceIter != sources.end(); ++sourceIter)
	{
		set<string> errors;

		// Did any error occur on this source ?
		unsigned int errorCount = history.getSourceItems(sourceIter->first, CrawlHistory::ERROR, errors);
		if (errorCount > 0)
		{
			Url urlObj(sourceIter->second);
			ustring sourceName(urlObj.getLocation());

			// Add the source
			TreeModel::iterator srcIter = m_refStore->append(errIter->children());
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
				statIter = m_refStore->append(srcIter->children());
				row = *statIter;
				row[m_statsColumns.m_name] = (*errorIter);
			}

			hasErrors = true;
		}
	}

	if (hasErrors == false)
	{
		statIter = m_refStore->append(errIter->children());
		row = *statIter;
		row[m_statsColumns.m_name] = _("None");
	}

	statisticsTreeview->expand_all();

	Adjustment *pAdjustement = statisticsScrolledwindow->get_hadjustment();
#ifdef DEBUG
	cout << "statisticsDialog: " << pAdjustement->get_value() << " "
		<< pAdjustement->get_lower() << " " << pAdjustement->get_upper() << endl;
#endif
	pAdjustement->set_value(pAdjustement->get_upper());
	statisticsScrolledwindow->set_hadjustment(pAdjustement);
	resize((int )pAdjustement->get_upper() * 2, get_height());
}
