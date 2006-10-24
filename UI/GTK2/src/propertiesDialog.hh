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
	ComboModelColumns m_languageColumns;
	Glib::RefPtr<Gtk::ListStore> m_refLanguageTree;
	LabelModelColumns m_labelsColumns;
	Glib::RefPtr<Gtk::ListStore> m_refLabelsTree;
	std::set<std::string> m_labels;
	bool m_editDocument;
	DocumentInfo m_docInfo;

	void populate_languageCombobox(const std::string &language, bool notALanguageName);

	void populate_labelsTreeview(const std::set<std::string> &docLabels);

	void on_labelOkButton_clicked();

};
#endif // _LABELDIALOG_HH
