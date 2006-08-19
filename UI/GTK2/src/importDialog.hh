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

#ifndef _IMPORTDIALOG_HH
#define _IMPORTDIALOG_HH

#include <string>
#include <set>
#include <sigc++/slot.h>
#include <sigc++/connection.h>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <gtkmm/button.h>
#include <gtkmm/liststore.h>

#include "DocumentInfo.h"
#include "MonitorInterface.h"
#include "ModelColumns.h"
#include "WorkerThreads.h"
#include "importDialog_glade.hh"

class importDialog : public importDialog_glade
{  
public:
	/// Open the dialog box in import mode.
	importDialog(const Glib::ustring &title);
	virtual ~importDialog();

	void setHeight(int maxHeight);

	unsigned int getDocumentsCount(void);

protected:
	void populate_comboboxes(bool localOnly);
	void signal_scanner(void);

	bool on_activity_timeout(void);
	bool on_import_url(const std::string &location, const std::string &extraLabel);
	void on_thread_end(WorkerThread *pThread);

private:
	MonitorInterface *m_pMonitor;
	// Type
	ComboModelColumns m_typeColumns;
	Glib::RefPtr<Gtk::ListStore> m_refTypeList;
	// Label
	ComboModelColumns m_labelNameColumns;
	Glib::RefPtr<Gtk::ListStore> m_refLabelNameTree;
	Glib::ustring m_title;
	std::string m_labelName;
	unsigned int m_docsCount;
	bool m_importDirectory;
	// Directory scanner
	DirectoryScannerThread *m_pScannerThread;
	// Activity timeout
	SigC::Connection m_timeoutConnection;
	// Internal state
	class InternalState : public ThreadsManager
	{
		public:
			InternalState(unsigned int maxIndexThreads, importDialog *pWindow);
			~InternalState();

			// Directory scanning
			bool m_importing;
			Glib::Mutex m_scanMutex;
			Glib::Cond m_scanCondVar;
			// The default directory
			static std::string m_defaultDirectory;

	} m_state;

	virtual void on_typeCombobox_changed();
	virtual void on_locationEntry_changed();
	virtual void on_locationButton_clicked();
	virtual void on_importButton_clicked();
	virtual void on_importDialog_response(int response_id);

};

#endif
