// generated 2003/5/18 21:15:37 BST by fabrice@amra.dyndns.org.(none)
// using glademm V2.0.0
//
// newer (non customized) versions of this file go to prefsDialog.hh_new

// you might replace
//    class foo : public foo_glade { ... };
// by
//    typedef foo_glade foo;
// if you didn't make any modifications to the widget

#ifndef _PREFSDIALOG_HH
#define _PREFSDIALOG_HH

#include <string>
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
	void on_message_import(DocumentInfo docInfo);

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
	std::set<Glib::ustring> m_deletedIndexes;
	std::set<std::string> m_deletedLabels;
	std::set<Glib::ustring> m_deletedMail;
	static unsigned int m_maxDirLevel;

};
#endif
