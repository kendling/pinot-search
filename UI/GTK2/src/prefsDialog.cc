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

unsigned int prefsDialog::m_maxDirLevel = 1;

prefsDialog::prefsDialog() :
	prefsDialog_glade(),
	m_settings(PinotSettings::getInstance())
{
	// Initialize widgets
	// Ignore robots directives
	ignoreRobotsCheckbutton->set_active(m_settings.m_ignoreRobotsDirectives);
	// Google API key
	if (m_settings.m_googleAPIKey.empty() == false)
	{
		apiKeyEntry->set_text(m_settings.m_googleAPIKey);
	}
	// New results colour
	newResultsColorbutton->set_color(m_settings.m_newResultsColour);
	// Enable terms suggestion
	enableCompletionCheckbutton->set_active(m_settings.m_suggestQueryTerms);

	// Associate the columns model to the labels tree
	m_refLabelsTree = ListStore::create(m_labelsColumns);
	labelsTreeview->set_model(m_refLabelsTree);
	labelsTreeview->append_column_editable(_("Name"), m_labelsColumns.m_name);
	// Allow only single selection
	labelsTreeview->get_selection()->set_mode(SELECTION_SINGLE);
	populate_labelsTreeview();

	// Associate the columns model to the mail accounts tree
	m_refMailTree = ListStore::create(m_mailColumns);
	mailTreeview->set_model(m_refMailTree);
	mailTreeview->append_column(_("Location"), m_mailColumns.m_location);
	mailTreeview->append_column(_("MIME Type"), m_mailColumns.m_type);
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

const set<string> &prefsDialog::getMailLabelsToDelete(void) const
{
	return m_deletedMail;
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

void prefsDialog::populate_mailTreeview()
{
	TreeModel::iterator iter;
	TreeModel::Row row;

	if (m_settings.m_mailAccounts.empty() == true)
	{
		// These buttons will stay disabled until labels are added to the list
		editAccountButton->set_sensitive(false);
		removeAccountButton->set_sensitive(false);
		return;
	}

	// Populate the tree
	for (set<PinotSettings::MailAccount>::iterator mailIter = m_settings.m_mailAccounts.begin();
		mailIter != m_settings.m_mailAccounts.end();
		++mailIter)
	{
		// Create a new row
		iter = m_refMailTree->append();
		row = *iter;
		// Set its name, type and minium date
		row[m_mailColumns.m_location] = mailIter->m_name;
		row[m_mailColumns.m_type] = mailIter->m_type;
		row[m_mailColumns.m_mTime] = mailIter->m_modTime;
		row[m_mailColumns.m_minDate] = mailIter->m_lastMessageTime;
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
			PinotSettings::MailAccount mailAccount;

			// FIXME: unlike libmagic, shared-mime-info fails to identify most mbox files
			// as being of type text/x-mail
			// Add this new mail account to the settings
			mailAccount.m_name = row[m_mailColumns.m_location];
			mailAccount.m_type = row[m_mailColumns.m_type];
			mailAccount.m_modTime = row[m_mailColumns.m_mTime];
			mailAccount.m_lastMessageTime = row[m_mailColumns.m_minDate];

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
	m_settings.m_newResultsColour = newResultsColorbutton->get_color();
	m_settings.m_suggestQueryTerms = enableCompletionCheckbutton->get_active();
	m_settings.m_googleAPIKey = apiKeyEntry->get_text();

	// Validate the current labels and mail accounts
	save_labelsTreeview();
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
		row[m_mailColumns.m_type] = to_utf8(mimeType);
		row[m_mailColumns.m_mTime] = 0;
		row[m_mailColumns.m_minDate] = 0;

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
			row[m_mailColumns.m_type] = to_utf8(MIMEScanner::scanFile(fileName));
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
