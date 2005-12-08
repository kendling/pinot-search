// generated 2003/6/13 20:26:49 BST by fabrice@amra.dyndns.org.(none)
// using glademm V2.0.0
//
// newer (non customized) versions of this file go to queryDialog.hh_new

// you might replace
//    class foo : public foo_glade { ... };
// by
//    typedef foo_glade foo;
// if you didn't make any modifications to the widget

#ifndef _QUERYDIALOG_HH
#define _QUERYDIALOG_HH

#include <string>
#include <set>
#include <gtkmm/liststore.h>

#include "QueryProperties.h"
#include "ModelColumns.h"
#include "PinotSettings.h"
#include "queryDialog_glade.hh"

class queryDialog : public queryDialog_glade
{
public:
	queryDialog(QueryProperties &properties);
	virtual ~queryDialog();

	bool badName(void) const;

protected:
	std::string m_name;
	QueryProperties& m_properties;
	const std::set<PinotSettings::Label> &m_labels;
	ComboModelColumns m_labelColumns;
	Glib::RefPtr<Gtk::ListStore> m_refLabelTree;
	ComboModelColumns m_languageColumns;
	Glib::RefPtr<Gtk::ListStore> m_refLanguageTree;
	bool m_badName;

	void populate_comboboxes();

	virtual void on_queryOkbutton_clicked();
	virtual void on_nameEntry_changed();

};
#endif
