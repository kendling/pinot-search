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
#include <iostream>
#include <algorithm>
#include <glibmm/convert.h>
#include <gdkmm/color.h>
#include <gtkmm/colorselection.h>
#include <gtkmm/menu.h>
#include <gtkmm/messagedialog.h>

#include "config.h"
#include "NLS.h"
#include "StringManip.h"
#include "ModuleFactory.h"
#include "PinotUtils.h"
#include "prefsDialog.hh"

using namespace std;
using namespace Glib;
using namespace Gdk;
using namespace Gtk;

prefsDialog::prefsDialog() :
	prefsDialog_glade(),
	m_settings(PinotSettings::getInstance()),
	m_startDaemon(false)
{
	Color newColour;

	newColour.set_red(m_settings.m_newResultsColourRed);
	newColour.set_green(m_settings.m_newResultsColourGreen);
	newColour.set_blue(m_settings.m_newResultsColourBlue);

	// Initialize widgets
	// Ignore robots directives
	ignoreRobotsCheckbutton->set_active(m_settings.m_ignoreRobotsDirectives);
	// Google API key
	if (m_settings.m_googleAPIKey.empty() == false)
	{
		apiKeyEntry->set_text(m_settings.m_googleAPIKey);
	}
	// New results colour
	newResultsColorbutton->set_color(newColour);
	// Enable terms suggestion
	enableCompletionCheckbutton->set_active(m_settings.m_suggestQueryTerms);

	populate_proxyTypeCombobox();
	proxyRadiobutton->set_active(m_settings.m_proxyEnabled);
	proxyAddressEntry->set_text(m_settings.m_proxyAddress);
	proxyPortSpinbutton->set_value((double)m_settings.m_proxyPort);
	int proxyType = 0;
	if (m_settings.m_proxyType == "SOCKS4")
	{
		proxyType = 1;
	}
	else if (m_settings.m_proxyType == "SOCKS5")
	{
		proxyType = 2;
	}
	proxyTypeCombobox->set_active(proxyType);
	on_directConnectionRadiobutton_toggled();

	// Associate the columns model to the labels tree
	m_refLabelsTree = ListStore::create(m_labelsColumns);
	labelsTreeview->set_model(m_refLabelsTree);
	labelsTreeview->append_column_editable(_("Name"), m_labelsColumns.m_name);
	// Allow only single selection
	labelsTreeview->get_selection()->set_mode(SELECTION_SINGLE);
	populate_labelsTreeview();

	// Associate the columns model to the directories tree
	m_refDirectoriesTree = ListStore::create(m_directoriesColumns);
	directoriesTreeview->set_model(m_refDirectoriesTree);
	directoriesTreeview->append_column_editable(_("Monitor"), m_directoriesColumns.m_monitor);
	directoriesTreeview->append_column(_("Location"), m_directoriesColumns.m_location);
	// Allow only single selection
	directoriesTreeview->get_selection()->set_mode(SELECTION_SINGLE);
	populate_directoriesTreeview();

	// Associate the columns model to the file patterns tree
	m_refPatternsTree = ListStore::create(m_patternsColumns);
	patternsTreeview->set_model(m_refPatternsTree);
	patternsTreeview->append_column_editable(_("Pattern"), m_patternsColumns.m_location);
	// Allow only single selection
	patternsTreeview->get_selection()->set_mode(SELECTION_SINGLE);
	populate_patternsCombobox();
	populate_patternsTreeview(m_settings.m_filePatternsList, m_settings.m_isBlackList);

	// Hide the Google API entry field ?
	if (ModuleFactory::isSupported("googleapi") == false)
	{
		apiKeyLabel->hide();
		apiKeyEntry->hide();
	}

	// Show The Indexing tab on first run
	if ((m_settings.isFirstRun() == true) &&
		(prefsNotebook != NULL) &&
		(prefsNotebook->get_n_pages() > 3))
	{
		prefsNotebook->set_current_page(3);
	}
}

prefsDialog::~prefsDialog()
{
}

const set<string> &prefsDialog::getLabelsToAdd(void) const
{
	return m_addedLabels;
}

const set<string> &prefsDialog::getLabelsToDelete(void) const
{
	return m_deletedLabels;
}

const map<string, string> &prefsDialog::getLabelsToRename(void) const
{
	return m_renamedLabels;
}

bool prefsDialog::startDaemon(void) const
{
	return m_startDaemon;
}

void prefsDialog::populate_proxyTypeCombobox()
{
	proxyTypeCombobox->append_text("HTTP");
	proxyTypeCombobox->append_text("SOCKS v4");
	proxyTypeCombobox->append_text("SOCKS v5");
}

void prefsDialog::populate_labelsTreeview()
{
	TreeModel::iterator iter;
	TreeModel::Row row;
	set<string> &labels = m_settings.m_labels;

	// Get labels directly from the daemon's index
	IndexInterface *pDaemonIndex = m_settings.getIndex(m_settings.m_daemonIndexLocation);
	if (pDaemonIndex != NULL)
	{
		set<string> indexLabels;

		// Nothing might be found if we are upgrading from an older version
		// and the daemon has not been run
		if (pDaemonIndex->getLabels(indexLabels) == true)
		{
			labels.clear();

			copy(indexLabels.begin(), indexLabels.end(),
				inserter(labels, labels.begin()));
		}
#ifdef DEBUG
		else cout << "prefsDialog::populate_labelsTreeview: relying on configuration file" << endl;
#endif

		delete pDaemonIndex;
	}

	if (labels.empty() == true)
	{
		// This button will stay disabled until labels are added to the list
		removeLabelButton->set_sensitive(false);
		return;
	}

	// Populate the tree
	for (set<string>::const_iterator labelIter = labels.begin();
		labelIter != labels.end();
		++labelIter)
	{
		// Create a new row
		iter = m_refLabelsTree->append();
		row = *iter;
		// Set its name
		row[m_labelsColumns.m_name] = *labelIter;
		row[m_labelsColumns.m_oldName] = *labelIter;
		// This allows to differentiate existing labels from new labels the user may create
		row[m_labelsColumns.m_enabled] = true;
	}

	removeLabelButton->set_sensitive(true);
}

void prefsDialog::save_labelsTreeview()
{
	set<string> &labels = m_settings.m_labels;

	labels.clear();

	// Go through the labels tree
	TreeModel::Children children = m_refLabelsTree->children();
	if (children.empty() == false)
	{
		TreeModel::Children::iterator iter = children.begin();
		for (; iter != children.end(); ++iter)
		{
			TreeModel::Row row = *iter;

			// Add this new label to the settings
			ustring labelName(row[m_labelsColumns.m_name]);
			ustring oldName(row[m_labelsColumns.m_oldName]);
			// Was this label renamed ?
			if ((row[m_labelsColumns.m_enabled] == true) &&
				(labelName != oldName))
			{
				// Yes, it was
				m_renamedLabels[from_utf8(oldName)] = from_utf8(labelName);
			}
			// Check user didn't recreate this label after having deleted it
			set<string>::iterator labelIter = m_deletedLabels.find(from_utf8(labelName));
			if (labelIter != m_deletedLabels.end())
			{
				m_deletedLabels.erase(labelIter);
			}
			// Is this a nw label ?
			if (row[m_labelsColumns.m_enabled] == false)
			{
				m_addedLabels.insert(from_utf8(labelName));
			}

#ifdef DEBUG
			cout << "prefsDialog::save_labelsTreeview: " << labelName << endl;
#endif
			labels.insert(labelName);
		}
	}
}

void prefsDialog::populate_directoriesTreeview()
{
	TreeModel::iterator iter;
	TreeModel::Row row;
	ustring dirsString;

	if (m_settings.m_indexableLocations.empty() == true)
	{
		// This button will stay disabled until directories are added to the list
		removeDirectoryButton->set_sensitive(false);
		return;
	}

	// Populate the tree
	for (set<PinotSettings::IndexableLocation>::iterator dirIter = m_settings.m_indexableLocations.begin();
		dirIter != m_settings.m_indexableLocations.end();
		++dirIter)
	{
		// Create a new row
		iter = m_refDirectoriesTree->append();
		row = *iter;
		row[m_directoriesColumns.m_monitor] = dirIter->m_monitor;
		row[m_directoriesColumns.m_location] = dirIter->m_name;
		dirsString += dirIter->m_name + (dirIter->m_monitor == true ? "1" : "0") + "|";
	}

	m_directoriesHash = StringManip::hashString(dirsString);
	removeDirectoryButton->set_sensitive(true);
}

bool prefsDialog::save_directoriesTreeview()
{
	string dirsString;

	// Clear the current settings
	m_settings.m_indexableLocations.clear();

	// Go through the directories tree
	TreeModel::Children children = m_refDirectoriesTree->children();
	if (children.empty() == false)
	{
		TreeModel::Children::iterator iter = children.begin();
		for (; iter != children.end(); ++iter)
		{
			TreeModel::Row row = *iter;
			PinotSettings::IndexableLocation indexableLocation;

			// Add this new directory to the settings
			indexableLocation.m_monitor = row[m_directoriesColumns.m_monitor];
			indexableLocation.m_name = row[m_directoriesColumns.m_location];

			string dirLabel("file://");
			dirLabel += from_utf8(indexableLocation.m_name);

			// Check user didn't recreate this directory after having deleted it
			set<string>::iterator dirIter = m_deletedDirectories.find(dirLabel);
			if (dirIter != m_deletedDirectories.end())
			{
				m_deletedDirectories.erase(dirIter);
			}

#ifdef DEBUG
			cout << "prefsDialog::save_directoriesTreeview: " << indexableLocation.m_name << endl;
#endif
			m_settings.m_indexableLocations.insert(indexableLocation);
			dirsString += indexableLocation.m_name + (indexableLocation.m_monitor == true ? "1" : "0") + "|";
		}
	}

	if (m_directoriesHash != StringManip::hashString(dirsString))
	{
#ifdef DEBUG
		cout << "prefsDialog::save_directoriesTreeview: directories changed" << endl;
#endif
		return true;
	}

	return false;
}

void prefsDialog::populate_patternsCombobox()
{
	patternsCombobox->append_text(_("Exclude these patterns from indexing"));
	patternsCombobox->append_text(_("Only index these patterns"));
}

void prefsDialog::populate_patternsTreeview(const set<ustring> &patternsList, bool isBlackList)
{
	TreeModel::iterator iter;
	TreeModel::Row row;
	ustring patternsString;

	if (patternsList.empty() == true)
	{
		// This button will stay disabled until a ppatern is added to the list
		removePatternButton->set_sensitive(false);
		return;
	}

	// Populate the tree
	for (set<ustring>::iterator patternIter = patternsList.begin();
		patternIter != patternsList.end();
		++patternIter)
	{
		ustring pattern(*patternIter);

		// Create a new row
		iter = m_refPatternsTree->append();
		row = *iter;
		// Set its name
		row[m_patternsColumns.m_location] = pattern;
		patternsString += pattern + "|";
	}

	removePatternButton->set_sensitive(true);

	// Is it a black or white list ?
	if (isBlackList == true)
	{
		patternsCombobox->set_active(0);
		patternsString += "0";
	}
	else
	{
		patternsCombobox->set_active(1);
		patternsString += "1";
	}

	m_patternsHash = StringManip::hashString(patternsString);
}

bool prefsDialog::save_patternsTreeview()
{
	ustring patternsString;

	// Clear the current settings
	m_settings.m_filePatternsList.clear();

	// Go through the file patterns tree
	TreeModel::Children children = m_refPatternsTree->children();
	if (children.empty() == false)
	{
		TreeModel::Children::iterator iter = children.begin();
		for (; iter != children.end(); ++iter)
		{
			TreeModel::Row row = *iter;
			ustring pattern(row[m_patternsColumns.m_location]);

			if (pattern.empty() == false)
			{
				m_settings.m_filePatternsList.insert(pattern);
				patternsString += pattern + "|";
			}
		}
	}
	if (patternsCombobox->get_active_row_number() == 0)
	{
		m_settings.m_isBlackList = true;
		patternsString += "0";
	}
	else
	{
		m_settings.m_isBlackList = false;
		patternsString += "1";
	}

	if (m_patternsHash != StringManip::hashString(patternsString))
	{
#ifdef DEBUG
		cout << "prefsDialog::save_patternsTreeview: patterns changed" << endl;
#endif
		return true;
	}

	return false;
}

void prefsDialog::on_prefsOkbutton_clicked()
{
	// Synchronise widgets with settings
	m_settings.m_ignoreRobotsDirectives = ignoreRobotsCheckbutton->get_active();
	Color newColour = newResultsColorbutton->get_color();
	m_settings.m_newResultsColourRed = newColour.get_red();
	m_settings.m_newResultsColourGreen = newColour.get_green();
	m_settings.m_newResultsColourBlue = newColour.get_blue();
	m_settings.m_suggestQueryTerms = enableCompletionCheckbutton->get_active();
	m_settings.m_googleAPIKey = apiKeyEntry->get_text();

	m_settings.m_proxyEnabled = proxyRadiobutton->get_active();
	m_settings.m_proxyAddress = proxyAddressEntry->get_text();
	m_settings.m_proxyPort = (unsigned int)proxyPortSpinbutton->get_value();
	int proxyType = proxyTypeCombobox->get_active_row_number();
	if (proxyType == 1)
	{
		m_settings.m_proxyType = "SOCKS4";
	}
	else if (proxyType == 2)
	{
		m_settings.m_proxyType = "SOCKS5";
	}
	else
	{
		m_settings.m_proxyType = "HTTP";
	}

	// Validate the current lists
	save_labelsTreeview();
	bool startForDirectories = save_directoriesTreeview();
	bool startForPatterns = save_patternsTreeview();
	if ((startForDirectories == true) ||
		(startForPatterns == true))
	{
		// Save the settings
		m_settings.save();
		m_startDaemon = true;
	}
}

void prefsDialog::on_directConnectionRadiobutton_toggled()
{
	bool enabled = proxyRadiobutton->get_active();

	proxyAddressEntry->set_sensitive(enabled);
	proxyPortSpinbutton->set_sensitive(enabled);
	proxyTypeCombobox->set_sensitive(enabled);
}

void prefsDialog::on_addLabelButton_clicked()
{
	// Now create a new entry in the labels list
	TreeModel::iterator iter = m_refLabelsTree->append();
	TreeModel::Row row = *iter;
	row[m_labelsColumns.m_name] = to_utf8(_("New Label"));
	// This marks the label as new
	row[m_labelsColumns.m_enabled] = false;

	// Enable this button
	removeLabelButton->set_sensitive(true);
}

void prefsDialog::on_removeLabelButton_clicked()
{
	// Get the selected label in the list
	TreeModel::iterator iter = labelsTreeview->get_selection()->get_selected();
	if (iter)
	{
		// Unselect
		labelsTreeview->get_selection()->unselect(iter);
		// Select another row
		TreeModel::Path labelPath = m_refLabelsTree->get_path(iter);
		labelPath.next();
		labelsTreeview->get_selection()->select(labelPath);
		// Erase
		TreeModel::Row row = *iter;
		m_deletedLabels.insert(from_utf8(row[m_labelsColumns.m_name]));
		m_refLabelsTree->erase(row);

		TreeModel::Children children = m_refLabelsTree->children();
		if (children.empty() == true)
		{
			// Disable this button
			removeLabelButton->set_sensitive(false);
		}
	}
}

void prefsDialog::on_addDirectoryButton_clicked()
{
	ustring dirName;

	TreeModel::Children children = m_refDirectoriesTree->children();
	bool wasEmpty = children.empty();

	if (select_file_name(_("Directory to index"), dirName, true, true) == true)
	{
#ifdef DEBUG
		cout << "prefsDialog::on_addDirectoryButton_clicked: "
			<< dirName << endl;
#endif
		// Create a new entry in the directories list
		TreeModel::iterator iter = m_refDirectoriesTree->append();
		TreeModel::Row row = *iter;
	
		row[m_directoriesColumns.m_monitor] = false;
		row[m_directoriesColumns.m_location] = to_utf8(dirName);

		if (wasEmpty == true)
		{
			// Enable this button
			removeDirectoryButton->set_sensitive(true);
		}
	}
}

void prefsDialog::on_removeDirectoryButton_clicked()
{
	// Get the selected directory in the list
	TreeModel::iterator iter = directoriesTreeview->get_selection()->get_selected();
	if (iter)
	{
		string dirLabel("file://");

		// Unselect
		directoriesTreeview->get_selection()->unselect(iter);
		// Select another row
		TreeModel::Path dirPath = m_refDirectoriesTree->get_path(iter);
		dirPath.next();
		directoriesTreeview->get_selection()->select(dirPath);

		// Erase
		TreeModel::Row row = *iter;
		dirLabel += from_utf8(row[m_directoriesColumns.m_location]);
		m_deletedDirectories.insert(dirLabel);
		m_refDirectoriesTree->erase(row);

		TreeModel::Children children = m_refDirectoriesTree->children();
		if (children.empty() == true)
		{
			// Disable this button
			removeDirectoryButton->set_sensitive(false);
		}
	}
}

void prefsDialog::on_patternsCombobox_changed()
{
	int activeRow = patternsCombobox->get_active_row_number();

	if (((activeRow == 0) && (m_settings.m_isBlackList == true)) ||
		((activeRow > 0) && (m_settings.m_isBlackList == false)))
	{
		// No change
		return;
	}

	// Unselect
	patternsTreeview->get_selection()->unselect_all();
	// Remove all patterns in order to make sure the user enters a new bunch
	m_refPatternsTree->clear();
}

void prefsDialog::on_addPatternButton_clicked()
{
	TreeModel::Children children = m_refPatternsTree->children();
	bool wasEmpty = children.empty();

	// Create a new entry in the file patterns list
	TreeModel::iterator iter = m_refPatternsTree->append();
	TreeModel::Row row = *iter;

	row[m_patternsColumns.m_location] = "";
	row[m_patternsColumns.m_mTime] = time(NULL);

	// Select and start editing the row
	TreeViewColumn *pColumn = patternsTreeview->get_column(0);
	if (pColumn != NULL)
	{
		TreeModel::Path patternPath = m_refPatternsTree->get_path(iter);
		patternsTreeview->set_cursor(patternPath, *pColumn, true);
	}

	if (wasEmpty == true)
	{
		// Enable this button
		removePatternButton->set_sensitive(true);
	}
}

void prefsDialog::on_removePatternButton_clicked()
{
	// Get the selected file pattern in the list
	TreeModel::iterator iter = patternsTreeview->get_selection()->get_selected();
	if (iter)
	{
		// Unselect
		patternsTreeview->get_selection()->unselect(iter);
		// Select another row
		TreeModel::Path patternPath = m_refPatternsTree->get_path(iter);
		patternPath.next();
		patternsTreeview->get_selection()->select(patternPath);

		// Erase
		TreeModel::Row row = *iter;
		m_refPatternsTree->erase(row);

		TreeModel::Children children = m_refPatternsTree->children();
		if (children.empty() == true)
		{
			// Disable this button
			removePatternButton->set_sensitive(false);
		}
	}
}

void prefsDialog::on_resetPatternsButton_clicked()
{
	set<ustring> defaultPatterns;
	bool isBlackList = m_settings.getDefaultPatterns(defaultPatterns);

	// Unselect
	patternsTreeview->get_selection()->unselect_all();
	// Remove all patterns
	m_refPatternsTree->clear();

	// Repopulate with defaults
	populate_patternsTreeview(defaultPatterns, isBlackList);
}

