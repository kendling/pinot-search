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
#include <sigc++/connection.h>
#include <glibmm/refptr.h>
#include <gdkmm/pixbuf.h>
#include <gdkmm/color.h>
#include <gtkmm/entrycompletion.h>
#include <gtkmm/rc.h>
#include <gtkmm/notebook.h>
#include <gtkmm/liststore.h>
#include <gtkmm/treestore.h>
#include <gtkmm/treeselection.h>
#include <gtkmm/selectiondata.h>
#include <gtkmm/menu.h>
#include <gtkmm/tooltips.h>

#include "DocumentInfo.h"
#include "IndexedDocument.h"
#include "QueryProperties.h"
#include "EnginesTree.h"
#include "HtmlView.h"
#include "ModelColumns.h"
#include "Notebook.h"
#include "PinotSettings.h"
#include "ResultsTree.h"
#include "WorkerThreads.h"
#include "mainWindow_glade.hh"

class mainWindow : public mainWindow_glade
{
public:
	mainWindow();
	virtual ~mainWindow();

	void set_status(const Glib::ustring &text, bool canBeSkipped = false);

protected:
	// Utility methods
	void populate_queryTreeview();
	void save_queryTreeview();
	void populate_indexMenu();

	// Handlers
	bool on_queryCompletion_match(const Glib::ustring &key, const Gtk::TreeModel::const_iterator &iter);
	void on_enginesTreeviewSelection_changed();
	void on_queryTreeviewSelection_changed();
	void on_resultsTreeviewSelection_changed(Glib::ustring queryName);
	void on_indexTreeviewSelection_changed(Glib::ustring indexName);
	void on_index_changed(Glib::ustring indexName);
	void on_label_changed(Glib::ustring indexName, Glib::ustring labelName);
	void on_switch_page(GtkNotebookPage *p0, guint p1);
	void on_close_page(Glib::ustring title, NotebookPageBox::PageType type);
	void on_thread_end();
	void on_editindex(Glib::ustring indexName, Glib::ustring location);
	void on_message_reception(DocumentInfo docInfo, std::string labelName);
	void on_message_indexupdate(IndexedDocument docInfo, unsigned int docId, std::string indexName);

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

	virtual void on_liveQueryEntry_changed();
	virtual void on_liveQueryEntry_activate();
	virtual void on_findButton_clicked();
	virtual void on_addQueryButton_clicked();
	virtual void on_editQueryButton_clicked();
	virtual void on_removeQueryButton_clicked();
	virtual void on_findQueryButton_clicked();

	virtual void on_indexBackButton_clicked(Glib::ustring indexName);
	virtual void on_indexForwardButton_clicked(Glib::ustring indexName);

	virtual bool on_queryTreeview_button_press_event(GdkEventButton *ev);
	virtual bool on_mainWindow_delete_event(GdkEventAny *ev);

	// Action methods
	NotebookPageBox *get_current_page(void);
	NotebookPageBox *get_page(const Glib::ustring &title,
		NotebookPageBox::PageType type);
	int get_page_number(const Glib::ustring &title,
		NotebookPageBox::PageType type);
	bool queue_index(const DocumentInfo &docInfo, const std::string &labelName,
		unsigned int docId = 0);
	void edit_query(QueryProperties &queryProps, bool newQuery);
	void run_search(const QueryProperties &queryProps);
	void browse_index(const Glib::ustring &indexName, unsigned int startDoc);
	void index_document(const DocumentInfo &docInfo, const std::string &labelName,
		unsigned int docId = 0);
	bool view_document(const std::string &url, bool internalViewerOnly = false);
	bool start_thread(WorkerThread *pNewThread, bool inBackground = false);
	bool check_queue(void);

	// Status methods
	bool on_activity_timeout(void);

private:
	// Global settings
	PinotSettings &m_settings;
	// Engine
	EnginesTree *m_pEnginesTree;
	// Query
	ComboModelColumns m_liveQueryColumns;
	Glib::RefPtr<Gtk::ListStore> m_refLiveQueryList;
	Glib::RefPtr<Gtk::EntryCompletion> m_refLiveQueryCompletion;
	QueryModelColumns m_queryColumns;
	Glib::RefPtr<Gtk::ListStore> m_refQueryTree;
	// Notebook
	Gtk::Notebook *m_pNotebook;
	// Index
	Gtk::Menu *m_pIndexMenu;
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
	class InternalState : public ThreadsManager
	{
		public:
			InternalState(mainWindow *pWindow);
			~InternalState();

			void connect(void);

			// Query
			unsigned int m_liveQueryLength;
			// Notebook pages
			int m_currentPage;
			// In-progress actions
			std::set<std::string> m_beingIndexed;
			bool m_browsingIndex;
			// Action queue
			std::map<DocumentInfo, string> m_indexQueue;

		protected:
			mainWindow *m_pMainWindow;

	} m_state;
	static unsigned int m_maxDocsCount;
	static unsigned int m_maxThreads;

};

#endif
