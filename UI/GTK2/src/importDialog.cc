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

#include <stdlib.h>
#include <strings.h>
#include <iostream>
#include <algorithm>
#include <utility>
#include <glibmm/convert.h>
#include <gtkmm/stock.h>

#include "config.h"
#include "DocumentInfo.h"
#include "MIMEScanner.h"
#include "NLS.h"
#include "Url.h"
#include "QueryHistory.h"
#include "PinotSettings.h"
#include "PinotUtils.h"
#include "importDialog.hh"

using namespace std;
using namespace Glib;
using namespace Gtk;

importDialog::InternalState::InternalState(unsigned int maxIndexThreads, importDialog *pWindow) :
	ThreadsManager(PinotSettings::getInstance().m_docsIndexLocation, maxIndexThreads),
	m_locationLength(0),
	m_importing(false)
{
	m_onThreadEndSignal.connect(sigc::mem_fun(*pWindow, &importDialog::on_thread_end));
}

importDialog::InternalState::~InternalState()
{
}

importDialog::importDialog() :
	importDialog_glade(),
	m_docsCount(0),
	m_state(10, this)
{
	// Enable completion on the location field
	m_refLocationList = ListStore::create(m_locationColumns);
	m_refLocationCompletion = EntryCompletion::create();
	m_refLocationCompletion->set_model(m_refLocationList);
	m_refLocationCompletion->set_text_column(m_locationColumns.m_name);
	m_refLocationCompletion->set_minimum_key_length(8);
	m_refLocationCompletion->set_popup_completion(true);
	locationEntry->set_completion(m_refLocationCompletion);

	// Populate
	populate_comboboxes();

	// Connect to threads' finished signal
	m_state.connect();

	// Disable this button as long the location entry field is empty
	// The title field may remain empty
	importButton->set_sensitive(false);
	importProgressbar->set_pulse_step(0.10);
}

importDialog::~importDialog()
{
	m_state.disconnect();
}

void importDialog::populate_comboboxes(void)
{
	labelNameCombobox->append_text(_("None"));
	labelNameCombobox->set_active(0);

	// Add all labels
	set<string> &labels = PinotSettings::getInstance().m_labels;
	for (set<string>::const_iterator labelIter = labels.begin();
		labelIter != labels.end(); ++labelIter)
	{
		labelNameCombobox->append_text(to_utf8(*labelIter));
	}
}

bool importDialog::on_activity_timeout(void)
{
	importProgressbar->pulse();

	return true;
}

void importDialog::import_url(const string &title, const string &location,
	const string &labelName)
{
	Url urlObj(location);
	DocumentInfo docInfo(from_utf8(title), location, MIMEScanner::scanUrl(urlObj), "");

	// Any label ?
	if (labelName.empty() == false)
	{
		set<string> labels;

		labels.insert(labelName);
		docInfo.setLabels(labels);
	}

	// Was the document queued for indexing ?
	ustring status = m_state.queue_index(docInfo);
	if (status.empty() == true)
	{
		m_timeoutConnection = Glib::signal_timeout().connect(sigc::mem_fun(*this,
			&importDialog::on_activity_timeout), 1000);
	}
	else
	{
		importProgressbar->set_text(status);
	}
}

void importDialog::on_thread_end(WorkerThread *pThread)
{
	ustring status;
	bool success = true;

	if (pThread == NULL)
	{
		return;
	}

	// Any thread still running ?
	if (m_state.has_threads() == false)
	{
#ifdef DEBUG
		cout << "importDialog::on_thread_end: imported all" << endl;
#endif
		m_state.m_importing = false;
	}

	// Did the thread fail ?
	status = pThread->getStatus();
	if (status.empty() == false)
	{
#ifdef DEBUG
		cout << "importDialog::on_thread_end: " << status << endl;
#endif
		success = false;
	}

	// What type of thread was it ?
	string type = pThread->getType();
	if (type == "IndexingThread")
	{
		// Did it succeed ?
		if (success == true)
		{
			// Yes, it did
			++m_docsCount;
		}
	}

	// Delete the thread
	delete pThread;

	// We might be able to run a queued action
	m_state.pop_queue();

	if (m_state.m_importing == false)
	{
		double fractionFilled = 1.0;

		m_timeoutConnection.block();
		m_timeoutConnection.disconnect();

		if (success == false)
		{
			importProgressbar->set_text(status);
			fractionFilled = 0.0;
		}
		importProgressbar->set_fraction(fractionFilled);
		importButton->set_sensitive(true);
	}
}

unsigned int importDialog::getDocumentsCount(void)
{
	return m_docsCount;
}

void importDialog::on_locationEntry_changed()
{
	ustring fileName = from_utf8(locationEntry->get_text());
	bool enableImport = true;

	if (fileName.empty() == false)
	{
		Url urlObj(fileName);

		// Check the URL is valid
		if (urlObj.getProtocol().empty() == true)
		{
			enableImport = false;
		}
		// FIXME: be more thorough
	}
	else
	{
		enableImport = false;
	}

	// Keep the button disabled as lon as we are importing
	if (m_state.m_importing == true)
	{
		enableImport = false;
	}
	importButton->set_sensitive(enableImport);

	unsigned int locationLength = fileName.length();

	// Reset the list
	m_refLocationList->clear();

	// If characters are being deleted or if the location is too short, don't
	// attempt to offer suggestions
	if ((m_state.m_locationLength > locationLength) ||
		(fileName.empty() == true) ||
		(m_refLocationCompletion->get_minimum_key_length() > fileName.length()))
	{
		m_state.m_locationLength = locationLength;
		return;
	}
	m_state.m_locationLength = locationLength;

	// Get 10 URLs like this one
	QueryHistory history(PinotSettings::getInstance().getHistoryDatabaseName());
	set<string> suggestedUrls;
	history.findUrlsLike(fileName, 10, suggestedUrls);
	// Populate the list
	for (set<string>::iterator urlIter = suggestedUrls.begin();
		urlIter != suggestedUrls.end(); ++urlIter)
	{
		TreeModel::iterator iter = m_refLocationList->append();
		TreeModel::Row row = *iter;

		row[m_locationColumns.m_name] = to_utf8(*urlIter);
	}
}

void importDialog::on_importButton_clicked()
{
	string title(from_utf8(titleEntry->get_text()));
	string location(from_utf8(locationEntry->get_text()));
	string labelName;

	// Rudimentary lock
	if (m_state.m_importing == true)
	{
		return;
	}
	m_state.m_importing = true;

	importProgressbar->set_text("");
	importProgressbar->set_fraction(0.0);
	// Disable the import button
	importButton->set_sensitive(false);

	// Label
	int chosenLabel = labelNameCombobox->get_active_row_number();
	if (chosenLabel > 0)
	{
		labelName = from_utf8(labelNameCombobox->get_active_text());
	}

	import_url(title, location, labelName);
}

void importDialog::on_importDialog_response(int response_id)
{
	if (m_timeoutConnection.connected() == true)
	{
		m_timeoutConnection.block();
		m_timeoutConnection.disconnect();
	}

	// Closing the window should stop everything
	m_state.disconnect();
	m_state.stop_threads();
}
