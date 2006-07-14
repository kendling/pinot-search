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
#include <strings.h>
#include <iostream>
#include <algorithm>
#include <utility>
#include <sigc++/class_slot.h>
#include <glibmm/convert.h>
#include <gtkmm/stock.h>

#include "config.h"
#include "MIMEScanner.h"
#include "NLS.h"
#include "Url.h"
#include "XapianIndex.h"
#include "PinotSettings.h"
#include "PinotUtils.h"
#include "importDialog.hh"

using namespace std;
using namespace Glib;
using namespace Gtk;

string importDialog::InternalState::m_defaultDirectory = "";

importDialog::InternalState::InternalState(unsigned int maxIndexThreads, importDialog *pWindow) :
	ThreadsManager(maxIndexThreads),
	m_importing(false)
{
	m_onThreadEndSignal.connect(SigC::slot(*pWindow, &importDialog::on_thread_end));
}

importDialog::InternalState::~InternalState()
{
}

importDialog::importDialog(const Glib::ustring &title) :
	importDialog_glade(),
	m_docsCount(0),
	m_importDirectory(false),
	m_pScannerThread(NULL),
	m_state(10, this)
{
	set_title(title);

	// Initialize the default directory
	if (m_state.m_defaultDirectory.empty() == true)
	{
		char *homeDir = getenv("HOME");
		if (homeDir != NULL)
		{
			m_state.m_defaultDirectory = homeDir + string("/");
		}
	}

	// Associate the columns model to the label combo
	m_refLabelNameTree = ListStore::create(m_labelNameColumns);
	labelNameCombobox->set_model(m_refLabelNameTree);
	labelNameCombobox->pack_start(m_labelNameColumns.m_name);
	// ...and to the type combo
	m_refTypeList = ListStore::create(m_typeColumns);
	typeCombobox->set_model(m_refTypeList);
	typeCombobox->pack_start(m_typeColumns.m_name);
	// Populate
	populate_comboboxes(false);

	// The default type is not directory
	// FIXME: this could also apply to URLs !
	depthSpinbutton->set_sensitive(false);
	depthSpinbutton->set_value(1);

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

void importDialog::populate_comboboxes(bool localOnly)
{
	bool foundLanguage = false;

	TreeModel::iterator iter = m_refTypeList->append();
	TreeModel::Row row = *iter;
	row[m_typeColumns.m_name] = _("File");
	iter = m_refTypeList->append();
	row = *iter;
	row[m_typeColumns.m_name] = _("Directory");
	if (localOnly == false)
	{
		iter = m_refTypeList->append();
		row = *iter;
		row[m_typeColumns.m_name] = _("URL");
	}
	typeCombobox->set_active(0);

	iter = m_refLabelNameTree->append();
	row = *iter;
	row[m_labelNameColumns.m_name] = _("None");
	labelNameCombobox->set_active(0);
	// Add all labels
	const set<string> &labels = PinotSettings::getInstance().getLabels();
	for (set<string>::const_iterator labelIter = labels.begin();
		labelIter != labels.end(); ++labelIter)
	{
		string labelName(*labelIter);

		iter = m_refLabelNameTree->append();
		row = *iter;
		row[m_labelNameColumns.m_name] = to_utf8(labelName);
	}
}

bool importDialog::start_thread(WorkerThread *pNewThread)
{
	if (m_state.start_thread(pNewThread, false) == false)
	{
		// Delete the object
		delete pNewThread;
		return false;
	}
#ifdef DEBUG
	cout << "importDialog::start_thread: started thread " << pNewThread->getId() << endl;
#endif

	return true;
}

void importDialog::signal_scanner(void)
{
	// Ask the scanner for another file
	m_state.m_scanMutex.lock();
	m_state.m_scanCondVar.signal();
	m_state.m_scanMutex.unlock();
#ifdef DEBUG
	cout << "importDialog::signal_scanner: signaled" << endl;
#endif
}

bool importDialog::on_activity_timeout(void)
{
	importProgressbar->pulse();

	return true;
}

bool importDialog::on_import_url(const string &location)
{
	XapianIndex index(PinotSettings::getInstance().m_indexLocation);
	IndexingThread *pThread = NULL;
	Url urlObj(location);
	set<string> labels;
	string title = from_utf8(m_title);
	unsigned int docId = 0;

	if (index.isGood() == true)
	{
		docId = index.hasDocument(location);
		index.getDocumentLabels(docId, labels);
	}
	labels.insert(m_labelName);

	if (m_importDirectory == true)
	{
		if (title.empty() == false)
		{
			title += " ";
		}
		title += urlObj.getFile();
	}

	DocumentInfo docInfo(title, location, MIMEScanner::scanUrl(urlObj), "");
	docInfo.setLabels(labels);

	if (docId > 0)
	{
		// This document needs updating
		index.getDocumentInfo(docId, docInfo);
		pThread = new IndexingThread(docInfo, docId);
	}
	else
	{
		pThread = new IndexingThread(docInfo);
	}

	// Launch the new thread
	start_thread(pThread);

	return false;
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
	if (type == "DirectoryScannerThread")
	{
		if (m_pScannerThread == dynamic_cast<DirectoryScannerThread *>(pThread))
		{
#ifdef DEBUG
			cout << "importDialog::on_thread_end: reset scanner" << endl;
#endif
			m_pScannerThread = NULL;
		}
	}
	else if (type == "IndexingThread")
	{
		// Did it succeed ?
		if (success == true)
		{
			// Yes, it did
			++m_docsCount;
		}

		if (m_state.m_importing == true)
		{
			signal_scanner();
		}
	}

	// Delete the thread
	delete pThread;

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

void importDialog::on_typeCombobox_changed()
{
	unsigned int type = typeCombobox->get_active_row_number();
	bool selectLocation = true;

	m_importDirectory = false;
	if (type == 1)
	{
		m_importDirectory = true;
	}
	else if (type > 1)
	{
		// Disable the select button only if type is URL
		selectLocation = false;
	}

	// See whether import should be enabled
	on_locationEntry_changed();

	// FIXME: this could also apply to URLs !
	depthSpinbutton->set_sensitive(m_importDirectory);
	locationButton->set_sensitive(selectLocation);
	linksCheckbutton->set_sensitive(selectLocation);
}

void importDialog::on_locationEntry_changed()
{
	ustring fileName = locationEntry->get_text();
	bool enableImport = true;

	if (fileName.empty() == false)
	{
		unsigned int type = typeCombobox->get_active_row_number();

		// Check the entry makes sense
		if (type <= 1)
		{
			if (fileName[0] != '/')
			{
				enableImport = false;
			}
		}
		else
		{
			Url urlObj(from_utf8(fileName));

			// Check the URL is valid
			if (urlObj.getProtocol().empty() == true)
			{
				enableImport = false;
			}
			// FIXME: be more thorough
		}
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
}

void importDialog::on_locationButton_clicked()
{
	ustring fileName = to_utf8(m_state.m_defaultDirectory);

	if (select_file_name(*this, _("Document To Import"), fileName, true, m_importDirectory) == true)
	{
		// Update the location
#ifdef DEBUG
		cout << "importDialog::on_locationButton_clicked: location is " << fileName << endl;
#endif
		locationEntry->set_text(fileName);
		ustring::size_type pos = fileName.find_last_of("/");
		if (pos != string::npos)
		{
			// Update the default directory
			m_state.m_defaultDirectory = from_utf8(fileName.substr(0, pos + 1));
#ifdef DEBUG
			cout << "importDialog::on_locationButton_clicked: directory now "
				<< m_state.m_defaultDirectory << endl;
#endif
		}
	}
}

void importDialog::on_importButton_clicked()
{
	string location = from_utf8(locationEntry->get_text());
	unsigned int level = 0;

	// Rudimentary lock
	if (m_state.m_importing == true)
	{
		return;
	}
	m_state.m_importing = true;

	importProgressbar->set_fraction(0.0);
	// Disable the import button
	importButton->set_sensitive(false);

	m_timeoutConnection = Glib::signal_timeout().connect(SigC::slot(*this,
		&importDialog::on_activity_timeout), 1000);

	// Title
	m_title = titleEntry->get_text();
	// Label
	m_labelName.clear();
	int chosenLabel = labelNameCombobox->get_active_row_number();
	if (chosenLabel > 0)
	{
		TreeModel::iterator iter = labelNameCombobox->get_active();
		TreeModel::Row row = *iter;
		m_labelName = from_utf8(row[m_labelNameColumns.m_name]);
	}
	// Type
	if (typeCombobox->get_active_row_number() <= 1)
	{
		// Maximum depth
		unsigned int maxDirLevel = (unsigned int)depthSpinbutton->get_value();

		// Scan the directory and import all its files
		m_pScannerThread = new DirectoryScannerThread(location,
			maxDirLevel, linksCheckbutton->get_active(),
			&m_state.m_scanMutex, &m_state.m_scanCondVar);
		m_pScannerThread->getFileFoundSignal().connect(SigC::slot(*this, &importDialog::on_import_url));
		start_thread(m_pScannerThread);
	}
	else
	{
		if (on_import_url(location) == true)
		{
			// It's asking for another file, so this one couldn't be indexed
			m_timeoutConnection.block();
			m_timeoutConnection.disconnect();

			importProgressbar->set_fraction(0.0);
			importButton->set_sensitive(true);
		}
	}
#ifdef DEBUG
	cout << "importDialog::on_importButton_clicked: done" << endl;
#endif
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
	if (m_pScannerThread != NULL)
	{
		m_pScannerThread->stop();
		signal_scanner();
	}
	m_state.stop_threads();
}
