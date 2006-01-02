// generated 2004/3/6 14:15:21 GMT by fabrice@amra.dyndns.org.(none)
// using glademm V2.0.0
//
// newer (non customized) versions of this file go to importDialog.hh_new

// you might replace
//    class foo : public foo_glade { ... };
// by
//    typedef foo_glade foo;
// if you didn't make any modifications to the widget

#ifndef _IMPORTDIALOG_HH
#define _IMPORTDIALOG_HH

#include <string>
#include <vector>
#include <sigc++/slot.h>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <gtkmm/button.h>
#include <gtkmm/liststore.h>

#include "DocumentInfo.h"
#include "ModelColumns.h"
#include "importDialog_glade.hh"

class importDialog : public importDialog_glade
{  
public:
	/// Open the dialog box in import mode.
	importDialog(const Glib::ustring &title,
		bool selectTitle = true, bool selectDepth = true,
		bool allowLocalOnly = false);
	virtual ~importDialog();

	Glib::ustring getDocumentTitle(void);

	unsigned int getDocumentsCount(void);

	/// Returns the import file signal.
	SigC::Signal1<void, DocumentInfo>& getImportFileSignal(void);

protected:
	void populate_combobox(bool allowLocalOnly);
	void scan_file(const std::string &fileName, unsigned int &level);
	void import_file(const std::string &fileName,
		const DocumentInfo &docInfo);

private:
	ComboModelColumns m_typeColumns;
	Glib::RefPtr<Gtk::ListStore> m_refTypeList;
	Glib::ustring m_title;
	unsigned int m_docsCount;
	bool m_importDirectory;
	unsigned int m_maxDirLevel;
	SigC::Signal1<void, DocumentInfo> m_signalImportFile;
	static std::string m_directory;

	virtual void on_importButton_clicked();
	virtual void on_selectButton_clicked();
	virtual void on_locationEntry_changed();
	virtual void on_typeCombobox_changed();

};

#endif
