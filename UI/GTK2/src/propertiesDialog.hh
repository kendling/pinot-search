// generated 2004/8/13 22:59:43 BST by fabrice@amra.dyndns.org.(none)
// using glademm V2.6.0_cvs
//
// newer (non customized) versions of this file go to propertiesDialog.hh_new

// you might replace
//    class foo : public foo_glade { ... };
// by
//    typedef foo_glade foo;
// if you didn't make any modifications to the widget

#ifndef _LABELDIALOG_HH
#define _LABELDIALOG_HH

#include <string>
#include <set>
#include <glibmm/refptr.h>
#include <gtkmm/liststore.h>

#include "DocumentInfo.h"
#include "ModelColumns.h"
#include "propertiesDialog_glade.hh"

class propertiesDialog : public propertiesDialog_glade
{  
public:
	propertiesDialog(const std::set<std::string> &docLabels,
		const DocumentInfo &docInfo, bool editDocument = true);
	virtual ~propertiesDialog();

	void setHeight(int maxHeight);

	const DocumentInfo &getDocumentInfo(void);

	const std::set<std::string> &getLabels(void) const;

protected:
	LabelModelColumns m_labelsColumns;
	Glib::RefPtr<Gtk::ListStore> m_refLabelsTree;
	std::set<std::string> m_labels;
	bool m_editDocument;
	DocumentInfo m_docInfo;

	void populate_labelsTreeview(const std::set<std::string> &docLabels);

	void on_labelOkButton_clicked();

};
#endif // _LABELDIALOG_HH
