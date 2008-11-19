/*
 *  Copyright 2005-2008 Fabrice Colin
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _MAINWINDOW_HH
#define _MAINWINDOW_HH

#include <string>
#include <set>
#include <sigc++/sigc++.h>
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
#include "QueryProperties.h"
#include "MonitorInterface.h"
#include "MonitorHandler.h"
#include "EnginesTree.h"
#include "IndexPage.h"
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
	void populate_queryTreeview(const std::string &selectedQueryName);
	void save_queryTreeview();
	void populate_cacheMenu();
	void populate_indexMenu();
	void populate_findMenu();
	void add_query(QueryProperties &queryProps, bool mergeQueries);
	bool get_results_page_details(const Glib::ustring &queryName,
		QueryProperties &queryProps, std::set<std::string> &locations);

	// Handlers
	void on_data_received(const Glib::RefPtr<Gdk::DragContext> &context,
		int x, int y, const Gtk::SelectionData &data, guint info, guint time);
	void on_enginesTreeviewSelection_changed();
	void on_queryTreeviewSelection_changed();
	void on_resultsTreeviewSelection_changed(Glib::ustring queryName);
	void on_indexTreeviewSelection_changed(Glib::ustring indexName);
	void on_index_changed(Glib::ustring indexName);
	void on_cache_changed(PinotSettings::CacheProvider cacheProvider);
	void on_searchagain_changed(Glib::ustring queryName);
	void on_query_changed(Glib::ustring indexName, Glib::ustring queryName);
	void on_switch_page(GtkNotebookPage *p0, guint p1);
	void on_close_page(Glib::ustring title, NotebookPageBox::PageType type);
	void on_thread_end(WorkerThread *pThread);
	void on_editindex(Glib::ustring indexName, Glib::ustring location);

	// Handlers inherited from the base class
	virtual void on_statistics_activate();
	virtual void on_configure_activate();
	virtual void on_quit_activate();

	virtual void on_cut_activate();
	virtual void on_copy_activate();
	virtual void on_paste_activate();
	virtual void on_delete_activate();

	virtual void on_clearresults_activate();
	virtual void on_showextract_activate();
	virtual void on_groupresults_activate();
	virtual void on_exportresults_activate();
	virtual void on_viewresults_activate();
	virtual void on_morelikethis_activate();
	virtual void on_indexresults_activate();

	virtual void on_import_activate();
	virtual void on_viewfromindex_activate();
	virtual void on_refreshindex_activate();
	virtual void on_unindex_activate();
	virtual void on_showfromindex_activate();

	virtual void on_about_activate();

	virtual void on_addIndexButton_clicked();
	virtual void on_removeIndexButton_clicked();

	virtual void on_enginesTogglebutton_toggled();

	virtual void on_liveQueryEntry_changed();
	virtual void on_liveQueryEntry_activate();
	virtual void on_findButton_clicked();
	virtual void on_addQueryButton_clicked();
	virtual void on_removeQueryButton_clicked();
	virtual void on_queryHistoryButton_clicked();
	virtual void on_findQueryButton_clicked();
	virtual void on_suggestQueryButton_clicked(Glib::ustring queryName, Glib::ustring queryText);

	virtual void on_indexBackButton_clicked(Glib::ustring indexName);
	virtual void on_indexForwardButton_clicked(Glib::ustring indexName);

	virtual bool on_queryTreeview_button_press_event(GdkEventButton *ev);
	virtual bool on_mainWindow_delete_event(GdkEventAny *ev);

	// Action methods
	void show_global_menuitems(bool showItems);
	void show_selectionbased_menuitems(bool showItems);
	NotebookPageBox *get_current_page(void);
	NotebookPageBox *get_page(const Glib::ustring &title,
		NotebookPageBox::PageType type);
	int get_page_number(const Glib::ustring &title,
		NotebookPageBox::PageType type);
	void edit_query(QueryProperties &queryProps, bool newQuery);
	void run_search(const QueryProperties &queryProps);
	void browse_index(const Glib::ustring &indexName, const Glib::ustring &labelName,
		unsigned int startDoc, bool changePage = true);
	void view_documents(const std::vector<DocumentInfo> &documentsList);
	bool append_document(IndexPage *pIndexPage, const Glib::ustring &indexName,
		const DocumentInfo &docInfo);
	bool start_thread(WorkerThread *pNewThread, bool inBackground = false);
	bool expand_locations(void);

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
	std::set<std::string> m_expandLocations;
	// Notebook
	Gtk::Notebook *m_pNotebook;
	// Menus
	Gtk::Menu *m_pIndexMenu;
	Gtk::Menu *m_pCacheMenu;
	Gtk::Menu *m_pFindMenu;
	// Index
	ComboModelColumns m_indexNameColumns;
	Glib::RefPtr<Gtk::ListStore> m_refIndexNameTree;
	// Tooltips
	Gtk::Tooltips m_tooltips;
	// Page switching
	sigc::connection m_pageSwitchConnection;
	// Activity timeout
	sigc::connection m_timeoutConnection;
	// Monitoring
	MonitorInterface *m_pSettingsMonitor;
	MonitorHandler *m_pSettingsHandler;
	// Internal state
	class InternalState : public ThreadsManager
	{
		public:
			InternalState(unsigned int maxIndexThreads, mainWindow *pWindow);
			virtual ~InternalState();

			// Query
			unsigned int m_liveQueryLength;
			// Notebook pages
			unsigned int m_currentPage;
			// Current actions
			bool m_browsingIndex;

	} m_state;
	static unsigned int m_maxDocsCount;
	static unsigned int m_maxIndexThreads;

};

#endif
