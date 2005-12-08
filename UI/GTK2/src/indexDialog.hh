// generated 2005/2/14 18:45:02 GMT by fabrice@amra.dyndns.org.(none)
// using glademm V2.6.0
//
// newer (non customized) versions of this file go to indexDialog.hh_new

// you might replace
//    class foo : public foo_glade { ... };
// by
//    typedef foo_glade foo;
// if you didn't make any modifications to the widget

#ifndef _INDEXDIALOG_HH
#define _INDEXDIALOG_HH

#include <glibmm/ustring.h>
#include <gtkmm/liststore.h>

#include "ModelColumns.h"
#include "indexDialog_glade.hh"

class indexDialog : public indexDialog_glade
{  
public:
	indexDialog();
	indexDialog(const Glib::ustring &name, const Glib::ustring &location);
	virtual ~indexDialog();

	bool badName(void) const;

	Glib::ustring getName(void) const;

	Glib::ustring getLocation(void) const;

protected:
	Glib::ustring m_name;
	Glib::ustring m_location;
	ComboModelColumns m_typeColumns;
	Glib::RefPtr<Gtk::ListStore> m_refTypeTree;
	bool m_editIndex;
	bool m_badName;

	void populate_typeCombobox(void);
	void checkFields(void);

	virtual void on_indexOkbutton_clicked();
	virtual void on_locationEntry_changed();
	virtual void on_locationButton_clicked();
	virtual void on_typeCombobox_changed();
	virtual void on_nameEntry_changed();

};
#endif
