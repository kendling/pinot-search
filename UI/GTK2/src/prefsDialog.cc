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
#include <iostream>
#include <glibmm/convert.h>
#include <gdkmm/color.h>
#include <gtkmm/colorselection.h>
#include <gtkmm/menu.h>
#include <gtkmm/messagedialog.h>

#include "MIMEScanner.h"
#include "SearchEngineFactory.h"
#include "QueryHistory.h"
#include "config.h"
#include "NLS.h"
#include "PinotUtils.h"
#include "prefsDialog.hh"

using namespace std;
using namespace Glib;
using namespace Gdk;
using namespace Gtk;

prefsDialog::prefsDialog() :
	prefsDialog_glade(),
	m_settings(PinotSettings::getInstance())
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
	directoriesTreeview->append_column(_("Location"), m_directoriesColumns.m_location);
	// Allow only single selection
	directoriesTreeview->get_selection()->set_mode(SELECTION_SINGLE);
	populate_directoriesTreeview();

	// Associate the columns model to the mail accounts tree
	m_refMailTree = ListStore::create(m_mailColumns);
	mailTreeview->set_model(m_refMailTree);
	mailTreeview->append_column(_("Location"), m_mailColumns.m_location);
	// Allow only single selection
	mailTreeview->get_selection()->set_mode(SELECTION_SINGLE);
	populate_mailTreeview();

	// Hide the Google API entry field ?
	if (SearchEngineFactory::isSupported("googleapi") == false)
	{
		apiKeyLabel->hide();
		apiKeyEntry->hide();
	}
}

prefsDialog::~prefsDialog()
{
}

const set<string> &prefsDialog::getLabelsToDelete(void) const
{
	return m_deletedLabels;
}

const map<string, string> &prefsDialog::getLabelsToRename(void) const
{
	return m_renamedLabels;
}

void prefsDialog::populate_labelsTreeview()
{
	TreeModel::iterator iter;
	TreeModel::Row row;

	const set<string> &labels = m_settings.getLabels();
	if (labels.empty() == true)
	{
		// These buttons will stay disabled until labels are added to the list
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

bool prefsDialog::save_labelsTreeview()
{
	// Clear the current settings
	m_settings.clearLabels();

	// Go through the labels tree
	TreeModel::Children children = m_refLabelsTree->children();
	if (children.empty() == false)
	{
		TreeModel::Children::iterator iter = children.begin();
		for (; iter != children.end(); ++iter)
		{
			TreeModel::Row row = *iter;

			// Add this new label to the settings
			ustring labelName = row[m_labelsColumns.m_name];
			ustring oldName = row[m_labelsColumns.m_oldName];
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

#ifdef DEBUG
			cout << "prefsDialog::save_labelsTreeview: " << labelName << endl;
#endif
			m_settings.addLabel(labelName);
		}
	}

	return true;
}

void prefsDialog::populate_directoriesTreeview()
{
	TreeModel::iterator iter;
	TreeModel::Row row;

	if (m_settings.m_indexableLocations.empty() == true)
	{
		// These buttons will stay disabled until directories are added to the list
		editDirectoryButton->set_sensitive(false);
		removeDirectoryButton->set_sensitive(false);
		return;
	}

	// Populate the tree
	for (set<PinotSettings::TimestampedItem>::iterator dirIter = m_settings.m_indexableLocations.begin();
		dirIter != m_settings.m_indexableLocations.end();
		++dirIter)
	{
		// Create a new row
		iter = m_refDirectoriesTree->append();
		row = *iter;
		// Set its name, type and minium date
		row[m_directoriesColumns.m_location] = dirIter->m_name;
		row[m_directoriesColumns.m_mTime] = dirIter->m_modTime;
	}

	editDirectoryButton->set_sensitive(true);
	removeDirectoryButton->set_sensitive(true);
}

bool prefsDialog::save_directoriesTreeview()
{
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
			PinotSettings::TimestampedItem indexableLocation;

			// Add this new directory to the settings
			indexableLocation.m_name = row[m_directoriesColumns.m_location];
			indexableLocation.m_modTime = row[m_directoriesColumns.m_mTime];

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
		}
	}

	return true;
}

void prefsDialog::populate_mailTreeview()
{
	TreeModel::iterator iter;
	TreeModel::Row row;

	if (m_settings.m_mailAccounts.empty() == true)
	{
		// These buttons will stay disabled until mail is added to the list
		editAccountButton->set_sensitive(false);
		removeAccountButton->set_sensitive(false);
		return;
	}

	// Populate the tree
	for (set<PinotSettings::TimestampedItem>::iterator mailIter = m_settings.m_mailAccounts.begin();
		mailIter != m_settings.m_mailAccounts.end();
		++mailIter)
	{
		// Create a new row
		iter = m_refMailTree->append();
		row = *iter;
		// Set its name, type and minium date
		row[m_mailColumns.m_location] = mailIter->m_name;
		row[m_mailColumns.m_mTime] = mailIter->m_modTime;
	}

	editAccountButton->set_sensitive(true);
	removeAccountButton->set_sensitive(true);
}

bool prefsDialog::save_mailTreeview()
{
	// Clear the current settings
	m_settings.m_mailAccounts.clear();

	// Go through the mail accounts tree
	TreeModel::Children children = m_refMailTree->children();
	if (children.empty() == false)
	{
		TreeModel::Children::iterator iter = children.begin();
		for (; iter != children.end(); ++iter)
		{
			TreeModel::Row row = *iter;
			PinotSettings::TimestampedItem mailAccount;

			// FIXME: unlike libmagic, shared-mime-info fails to identify most mbox files
			// as being of type text/x-mail
			// Add this new mail account to the settings
			mailAccount.m_name = row[m_mailColumns.m_location];
			mailAccount.m_modTime = row[m_mailColumns.m_mTime];

			string mailLabel("mailbox://");
			mailLabel += from_utf8(mailAccount.m_name);

			// Check user didn't recreate this mail account after having deleted it
			set<string>::iterator mailIter = m_deletedMail.find(mailLabel);
			if (mailIter != m_deletedMail.end())
			{
				m_deletedMail.erase(mailIter);
			}

#ifdef DEBUG
			cout << "prefsDialog::save_mailTreeview: " << mailAccount.m_name << endl;
#endif
			m_settings.m_mailAccounts.insert(mailAccount);
		}
	}

	return true;
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

	// Validate the current lists
	save_labelsTreeview();
	save_directoriesTreeview();
	save_mailTreeview();
}

void prefsDialog::on_addLabelButton_clicked()
{
	// Now create a new entry in the labels list
	TreeModel::iterator iter = m_refLabelsTree->append();
	TreeModel::Row row = *iter;
	row[m_labelsColumns.m_name] = to_utf8(_("New Label"));
	// This marks the label as new
	row[m_labelsColumns.m_enabled] = false;

	// Enable these buttons
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
		TreeModel::Path path = m_refLabelsTree->get_path(iter);
		path.next();
		labelsTreeview->get_selection()->select(path);
		// Erase
		TreeModel::Row row = *iter;
		m_deletedLabels.insert(from_utf8(row[m_labelsColumns.m_name]));
		m_refLabelsTree->erase(row);

		TreeModel::Children children = m_refLabelsTree->children();
		if (children.empty() == true)
		{
			// Disable these buttons
			removeLabelButton->set_sensitive(false);
		}
	}
}

bool prefsDialog::on_mailTreeview_button_press_event(GdkEventButton *ev)
{
	// Check for double clicks
	if (ev->type == GDK_2BUTTON_PRESS)
	{
		on_editAccountButton_clicked();
	}

	return false;
}

void prefsDialog::on_addDirectoryButton_clicked()
{
	ustring dirName;

	TreeModel::Children children = m_refDirectoriesTree->children();
	bool wasEmpty = children.empty();

	if (select_file_name(*this, _("Directory to index"), dirName, true, true) == true)
	{
#ifdef DEBUG
		cout << "prefsDialog::on_addDirectoryButton_clicked: "
			<< dirName << endl;
#endif
		// Create a new entry in the directories list
		TreeModel::iterator iter = m_refDirectoriesTree->append();
		TreeModel::Row row = *iter;
	
		row[m_directoriesColumns.m_location] = to_utf8(dirName);
		row[m_directoriesColumns.m_mTime] = time(NULL);

		if (wasEmpty == true)
		{
			// Enable these buttons
			editDirectoryButton->set_sensitive(true);
			removeDirectoryButton->set_sensitive(true);
		}
	}
}

void prefsDialog::on_editDirectoryButton_clicked()
{
	// Get the selected directory in the list
	TreeModel::iterator iter = directoriesTreeview->get_selection()->get_selected();
	if (iter)
	{
		TreeModel::Row row = *iter;
		ustring dirName = row[m_directoriesColumns.m_location];
		// Let the user edit the location
		if (select_file_name(*this, _("Directory to index"), dirName, true, true) == true)
		{
			row[m_directoriesColumns.m_location] = dirName;
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
		TreeModel::Path path = m_refDirectoriesTree->get_path(iter);
		path.next();
		directoriesTreeview->get_selection()->select(path);

		// Erase
		TreeModel::Row row = *iter;
		dirLabel += from_utf8(row[m_directoriesColumns.m_location]);
		m_deletedDirectories.insert(dirLabel);
		m_refDirectoriesTree->erase(row);

		TreeModel::Children children = m_refDirectoriesTree->children();
		if (children.empty() == true)
		{
			// Disable these buttons
			editDirectoryButton->set_sensitive(false);
			removeDirectoryButton->set_sensitive(false);
		}
	}
}

void prefsDialog::on_addAccountButton_clicked()
{
	ustring fileName;

	TreeModel::Children children = m_refMailTree->children();
	bool wasEmpty = children.empty();

	if (select_file_name(*this, _("Mbox File Location"), fileName, true) == true)
	{
		string mimeType = MIMEScanner::scanFile(fileName);

#ifdef DEBUG
		cout << "prefsDialog::on_addAccountButton_clicked: " << fileName
			<< " has format " << mimeType << endl;
#endif
		// FIXME: unlike libmagic, shared-mime-info fails to identify most mbox files
		// as being of type text/x-mail
		// Create a new entry in the mail accounts list
		TreeModel::iterator iter = m_refMailTree->append();
		TreeModel::Row row = *iter;
	
		row[m_mailColumns.m_location] = to_utf8(fileName);
		row[m_mailColumns.m_mTime] = time(NULL);

		if (wasEmpty == true)
		{
			// Enable these buttons
			editAccountButton->set_sensitive(true);
			removeAccountButton->set_sensitive(true);
		}
	}
}

void prefsDialog::on_editAccountButton_clicked()
{
	// Get the selected mail account in the list
	TreeModel::iterator iter = mailTreeview->get_selection()->get_selected();
	if (iter)
	{
		TreeModel::Row row = *iter;
		ustring fileName = row[m_mailColumns.m_location];
		// Let the user edit the location
		if (select_file_name(*this, _("Mbox File Location"), fileName, true) == true)
		{
			row[m_mailColumns.m_location] = fileName;
		}
	}
}

void prefsDialog::on_removeAccountButton_clicked()
{
	// Get the selected mail account in the list
	TreeModel::iterator iter = mailTreeview->get_selection()->get_selected();
	if (iter)
	{
		string mailLabel("mailbox://");

		// Unselect
		mailTreeview->get_selection()->unselect(iter);
		// Select another row
		TreeModel::Path path = m_refMailTree->get_path(iter);
		path.next();
		mailTreeview->get_selection()->select(path);

		// Erase
		TreeModel::Row row = *iter;
		mailLabel += from_utf8(row[m_mailColumns.m_location]);
		m_deletedMail.insert(mailLabel);
		m_refMailTree->erase(row);

		TreeModel::Children children = m_refMailTree->children();
		if (children.empty() == true)
		{
			// Disable these buttons
			editAccountButton->set_sensitive(false);
			removeAccountButton->set_sensitive(false);
		}
	}
}
