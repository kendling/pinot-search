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

#ifndef _MAINWINDOW_HH
#define _MAINWINDOW_HH

#include <string>
#include <map>
#include <set>
#include <pthread.h>
#include <glibmm/refptr.h>
#include <gdkmm/pixbuf.h>
#include <gdkmm/color.h>
#include <gtkmm/rc.h>
#include <gtkmm/liststore.h>
#include <gtkmm/treestore.h>
#include <gtkmm/treeselection.h>
#include <gtkmm/selectiondata.h>
#include <gtkmm/menu.h>
#include <gtkmm/tooltips.h>

#include "DocumentInfo.h"
#include "IndexedDocument.h"
#include "ActionHistory.h"
#include "QueryProperties.h"
#include "HtmlView.h"
#include "EnginesTree.h"
#include "IndexTree.h"
#include "ModelColumns.h"
#include "PinotSettings.h"
#include "ResultsTree.h"
#include "WorkerThreads.h"
#include "mainWindow_glade.hh"

class mainWindow : public mainWindow_glade
{
public:
	mainWindow();
	virtual ~mainWindow();

protected:
	// Utility methods
	void populate_queryTreeview();
	void save_queryTreeview();
	void populate_indexCombobox();
	void populate_labelMenu();

	// Handlers
	void on_enginesTreeviewSelection_changed();
	void on_queryTreeviewSelection_changed();
	void on_resultsTreeviewSelection_changed();
	void on_indexTreeviewSelection_changed();
	void on_labelMenu_changed(unsigned int pos);
	void on_thread_end();
	void on_editindex(Glib::ustring indexName, Glib::ustring location);
	void on_message_reception(DocumentInfo docInfo, std::string labelName);
	void on_message_indexupdate(IndexedDocument docInfo, unsigned int docId, std::string indexName);
	void on_message_import(DocumentInfo docInfo);

	// Handlers inherited from the base class
	virtual void on_configure_activate();
	virtual void on_quit_activate();

	virtual void on_cut_activate();
	virtual void on_copy_activate();
	virtual void on_paste_activate();
	virtual void on_delete_activate();

	virtual void on_clearresults_activate();
	virtual void on_showextract_activate();
	virtual void on_groupresults_activate();
	virtual void on_viewresults_activate();
	virtual void on_viewcache_activate();
	virtual void on_indexresults_activate();

	virtual void on_import_activate();
	virtual void on_viewfromindex_activate();
	virtual void on_refreshindex_activate();
	virtual void on_showfromindex_activate();
	virtual void on_unindex_activate();

	virtual void on_about_activate();

	virtual void on_addIndexButton_clicked();
	virtual void on_removeIndexButton_clicked();

	virtual void on_findButton_clicked();
	virtual void on_addQueryButton_clicked();
	virtual void on_editQueryButton_clicked();
	virtual void on_removeQueryButton_clicked();
	virtual void on_findQueryButton_clicked();

	virtual void on_indexCombobox_changed();

	virtual void on_indexFirstButton_clicked();
	virtual void on_indexBackButton_clicked();
	virtual void on_indexForwardButton_clicked();
	virtual void on_indexLastButton_clicked();

	virtual bool on_queryTreeview_button_press_event(GdkEventButton *ev);
	virtual bool on_mainWindow_delete_event(GdkEventAny *ev);

	// Action methods
	bool queue_index(const DocumentInfo &docInfo, const std::string &labelName,
		unsigned int docId = 0);
	bool queue_unindex(set<unsigned int> &docIdList);
	void run_search(const QueryProperties &queryProps);
	void browse_index(unsigned int startDoc = 0);
	void index_document(const DocumentInfo &docInfo, const std::string &labelName,
		unsigned int docId = 0);
	bool view_document(const std::string &url, bool internalViewerOnly = false);
	void start_thread(WorkerThread *pNewThread, bool inBackground = false);
	bool check_queue(void);

	// Status methods
	bool on_activity_timeout(void);
	unsigned int get_threads_count(void);
	void update_threads_status(void);
	void set_status(const Glib::ustring &text, bool canBeSkipped = false);

private:
	// Threads status text
	Glib::ustring m_threadStatusText;
	// Global settings
	PinotSettings &m_settings;
	// Engine
	EnginesTree *m_pEnginesTree;
	// Query
	QueryModelColumns m_queryColumns;
	Glib::RefPtr<Gtk::ListStore> m_refQueryTree;
	// Results
	ResultsTree *m_pResultsTree;
	// Index
	IndexTree *m_pIndexTree;
	Gtk::Menu *m_pLabelsMenu;
	ComboModelColumns m_indexNameColumns;
	Glib::RefPtr<Gtk::ListStore> m_refIndexNameTree;
	// View
	HtmlView *m_pHtmlView;
	// Tooltips
	Gtk::Tooltips m_tooltips;
	// Activity timeout
	SigC::Connection m_timeoutConnection;
	// Internal state
	struct InternalState
	{
		public:
			InternalState();
			~InternalState();

			bool readLock(unsigned int where);
			bool writeLock(unsigned int where);
			void unlock(void);

			unsigned int getCurrentLabel(std::string &labelName);
			void setCurrentLabel(unsigned int labelPos, const std::string &labelName);
			Glib::ustring getCurrentIndex(void);
			void setCurrentIndex(const Glib::ustring &indexName);

			// Index
			unsigned int m_indexDocsCount;
			unsigned int m_startDoc;
			// Worker threads
			std::set<WorkerThread *> m_pThreads;
			unsigned int m_backgroundThreads;
			// In-progress actions
			std::set<std::string> m_beingIndexed;
			bool m_browsingIndex;

		protected:
			// Read/write lock
			pthread_rwlock_t m_rwLock;
			// Index
			unsigned int m_currentLabelPos;
			std::string m_currentLabelName;
			Glib::ustring m_currentIndexName;

	} m_state;
	static unsigned int m_maxDocsCount;
	static unsigned int m_maxThreads;

};

#endif
