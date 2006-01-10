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

#ifndef _PREFSDIALOG_HH
#define _PREFSDIALOG_HH

#include <string>
#include <map>
#include <set>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <gtkmm/liststore.h>

#include "ModelColumns.h"
#include "PinotSettings.h"
#include "prefsDialog_glade.hh"

class prefsDialog : public prefsDialog_glade
{  
public:
	prefsDialog();
	virtual ~prefsDialog();

	const std::set<std::string> &getLabelsToDelete(void) const;

	const std::map<std::string, std::string> &getLabelsToRename(void) const;

	const std::set<std::string> &getMailLabelsToDelete(void) const;

protected:
	virtual void on_prefsOkbutton_clicked();
	virtual void on_viewCombobox_changed();
	virtual void on_browserButton_clicked();
	virtual void on_addLabelButton_clicked();
	virtual void on_editLabelButton_clicked();
	virtual void on_removeLabelButton_clicked();
	virtual bool on_mailTreeview_button_press_event(GdkEventButton *ev);
	virtual void on_addAccountButton_clicked();
	virtual void on_editAccountButton_clicked();
	virtual void on_removeAccountButton_clicked();

	void populate_comboboxes();
	void populate_labelsTreeview();
	bool save_labelsTreeview();
	void populate_mailTreeview();
	bool save_mailTreeview();

private:
	PinotSettings &m_settings;
	ComboModelColumns m_viewColumns;
	Glib::RefPtr<Gtk::ListStore> m_refViewTree;
	OtherIndexModelColumns m_otherIndexColumns;
	Glib::RefPtr<Gtk::ListStore> m_refOtherIndexTree;
	LabelModelColumns m_labelsColumns;
	Glib::RefPtr<Gtk::ListStore> m_refLabelsTree;
	MailAccountModelColumns m_mailColumns;
	Glib::RefPtr<Gtk::ListStore> m_refMailTree;
	std::set<std::string> m_deletedLabels;
	std::map<std::string, std::string> m_renamedLabels;
	std::set<std::string> m_deletedMail;
	static unsigned int m_maxDirLevel;

};
#endif
