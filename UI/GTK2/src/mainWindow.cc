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

#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <string>
#include <iostream>
#include <fstream>
#include <list>
#include <sigc++/class_slot.h>
#include <glibmm/ustring.h>
#include <glibmm/stringutils.h>
#include <glibmm/convert.h>
#include <glibmm/thread.h>
#include <gtkmm/aboutdialog.h>
#include <gtkmm/stock.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/clipboard.h>
#include <gtkmm/main.h>

#include "CommandLine.h"
#include "StringManip.h"
#include "TimeConverter.h"
#include "MIMEScanner.h"
#include "Url.h"
#include "XapianDatabase.h"
#include "QueryHistory.h"
#include "ViewHistory.h"
#include "DownloaderFactory.h"
#include "SearchEngineFactory.h"
#include "config.h"
#include "NLS.h"
#include "PinotUtils.h"
#include "mainWindow.hh"
#include "importDialog.hh"
#include "indexDialog.hh"
#include "launcherDialog.hh"
#include "propertiesDialog.hh"
#include "prefsDialog.hh"
#include "queryDialog.hh"
#include "statisticsDialog.hh"

using namespace std;
using namespace Glib;
using namespace Gdk;
using namespace Gtk;

// FIXME: this ought to be configurable
unsigned int mainWindow::m_maxDocsCount = 100;
unsigned int mainWindow::m_maxIndexThreads = 2;

mainWindow::InternalState::InternalState(unsigned int maxIndexThreads, mainWindow *pWindow) :
	ThreadsManager(PinotSettings::getInstance().m_docsIndexLocation, maxIndexThreads),
	m_liveQueryLength(0),
	m_currentPage(0),
	m_browsingIndex(false)
{
	m_onThreadEndSignal.connect(SigC::slot(*pWindow, &mainWindow::on_thread_end));
}

mainWindow::InternalState::~InternalState()
{
}

//
// Constructor
//
mainWindow::mainWindow() :
	mainWindow_glade(),
	m_settings(PinotSettings::getInstance()),
	m_pEnginesTree(NULL),
	m_pNotebook(NULL),
	m_pIndexMenu(NULL),
	m_pCacheMenu(NULL),
	m_state(m_maxIndexThreads, this)
{
	// Reposition and resize the window
	// Make sure the coordinates and sizes make sense
	if ((m_settings.m_xPos >= 0) &&
		(m_settings.m_yPos >= 0))
	{
		move(m_settings.m_xPos, m_settings.m_yPos);
	}
	if ((m_settings.m_width > 0) &&
		(m_settings.m_height > 0))
	{
		resize(m_settings.m_width, m_settings.m_height);
	}
	if (m_settings.m_panePos >= 0)
	{
		mainHpaned->set_position(m_settings.m_panePos);
	}
	queryExpander->set_expanded(m_settings.m_expandQueries);
	enginesTogglebutton->set_active(m_settings.m_showEngines);
	if (m_settings.m_showEngines == true)
	{
		leftVbox->show();
	}

	// Position the engine tree
	m_pEnginesTree = manage(new EnginesTree(enginesVbox, m_settings));
	// Connect to the "changed" signal
	m_pEnginesTree->get_selection()->signal_changed().connect(
		SigC::slot(*this, &mainWindow::on_enginesTreeviewSelection_changed));
	// Connect to the edit index signal
	m_pEnginesTree->getEditIndexSignal().connect(
		SigC::slot(*this, &mainWindow::on_editindex));

	// Enable completion on the live query field
	m_refLiveQueryList = ListStore::create(m_liveQueryColumns);
	m_refLiveQueryCompletion = EntryCompletion::create();
	m_refLiveQueryCompletion->set_model(m_refLiveQueryList);
	m_refLiveQueryCompletion->set_text_column(m_liveQueryColumns.m_name);
	m_refLiveQueryCompletion->set_minimum_key_length(3);
	m_refLiveQueryCompletion->set_popup_completion(true);
	m_refLiveQueryCompletion->set_match_func(
		SigC::slot(*this, &mainWindow::on_queryCompletion_match));
	liveQueryEntry->set_completion(m_refLiveQueryCompletion);

	// Associate the columns model to the query tree
	m_refQueryTree = ListStore::create(m_queryColumns);
	queryTreeview->set_model(m_refQueryTree);
	TreeViewColumn *pColumn = create_column(_("Query Name"), m_queryColumns.m_name, true, true, m_queryColumns.m_name);
	if (pColumn != NULL)
	{
		queryTreeview->append_column(*manage(pColumn));
	}
	pColumn = create_column(_("Last Run"), m_queryColumns.m_lastRun, true, true, m_queryColumns.m_lastRunTime);
	if (pColumn != NULL)
	{
		queryTreeview->append_column(*manage(pColumn));
	}
	queryTreeview->append_column(_("Summary"), m_queryColumns.m_summary);
	// Allow only single selection
	queryTreeview->get_selection()->set_mode(SELECTION_SINGLE);
	// Connect to the "changed" signal
	queryTreeview->get_selection()->signal_changed().connect(
		SigC::slot(*this, &mainWindow::on_queryTreeviewSelection_changed));
	// Populate
	populate_queryTreeview("");

	// Populate the menus
	populate_menus();

	// Create a notebook, without any page initially
	m_pNotebook = manage(new Notebook());
	m_pNotebook->set_flags(Gtk::CAN_FOCUS);
	m_pNotebook->set_show_tabs(true);
	m_pNotebook->set_show_border(true);
	m_pNotebook->set_tab_pos(Gtk::POS_TOP);
	m_pNotebook->set_scrollable(false);
	rightVbox->pack_start(*m_pNotebook, Gtk::PACK_EXPAND_WIDGET, 4);
	m_pageSwitchConnection = m_pNotebook->signal_switch_page().connect(
		SigC::slot(*this, &mainWindow::on_switch_page), false);

	// Gray out menu items
	editQueryButton->set_sensitive(false);
	removeQueryButton->set_sensitive(false);
	findQueryButton->set_sensitive(false);
	show_global_menuitems(false);
	show_selectionbased_menuitems(false);
	// ...and buttons
	removeIndexButton->set_sensitive(false);

	// Set tooltips
	m_tooltips.set_tip(*enginesTogglebutton, _("Show all search engines"));
	m_tooltips.set_tip(*addIndexButton, _("Add index"));
	m_tooltips.set_tip(*removeIndexButton, _("Remove index"));

	// Connect to threads' finished signal
	m_state.connect();

	// Now we are ready
	set_status(_("Ready"));
	m_pNotebook->show();
	show();
	liveQueryEntry->grab_focus();

	// Open the preferences on first run
	if (m_settings.isFirstRun() == true)
	{
		on_configure_activate();
	}
}

//
// Destructor
//
mainWindow::~mainWindow()
{
	// FIXME: delete all "ignored" threads when exiting !!!

	// Save engines
	m_pEnginesTree->save();

	// Save queries
	save_queryTreeview();
}

//
// Load user-defined queries
//
void mainWindow::populate_queryTreeview(const string &selectedQueryName)
{
	QueryHistory history(m_settings.m_historyDatabase);
	const std::map<string, QueryProperties> &queries = m_settings.getQueries();

	// Reset the whole tree
	queryTreeview->get_selection()->unselect_all();
	m_refQueryTree->clear();

	// Add all user-defined queries
	for (std::map<string, QueryProperties>::const_iterator queryIter = queries.begin();
		queryIter != queries.end(); ++queryIter)
	{
		TreeModel::iterator iter = m_refQueryTree->append();
		TreeModel::Row row = *iter;
		string queryName = queryIter->first;
		string lastRun = history.getLastRun(queryName);
		time_t lastRunTime = 0;

		row[m_queryColumns.m_name] = to_utf8(queryName);
		if (lastRun.empty() == true)
		{
			lastRun = _("N/A");
		}
		else
		{
			lastRunTime = TimeConverter::fromTimestamp(lastRun);
		}
		row[m_queryColumns.m_lastRun] = to_utf8(lastRun);
		row[m_queryColumns.m_lastRunTime] = lastRunTime;
		string summary(StringManip::replaceSubString(queryIter->second.getFreeQuery(), "\n", " "));
		if (summary.empty() == false)
		{
			row[m_queryColumns.m_summary] = to_utf8(summary);
		}
		else
		{
			row[m_queryColumns.m_summary] = _("Undefined");
		}
		row[m_queryColumns.m_properties] = queryIter->second;

		if ((selectedQueryName.empty() == false) &&
			(queryName == selectedQueryName))
		{
			// Select this query
			TreeModel::Path path = m_refQueryTree->get_path(iter);
			queryTreeview->get_selection()->select(path);
		}
	}
}

//
// Store defined queries into the settings object
//
void mainWindow::save_queryTreeview()
{
	// Clear the current queries
	m_settings.clearQueries();

	// Go through the query tree
	TreeModel::Children children = m_refQueryTree->children();
	if (children.empty() == false)
	{
		for (TreeModel::Children::iterator iter = children.begin();
			iter != children.end(); ++iter)
		{
			TreeModel::Row row = *iter;

			// Add this query to the settings
			string name = from_utf8(row[m_queryColumns.m_name]);
			QueryProperties queryProps = row[m_queryColumns.m_properties];
#ifdef DEBUG
			cout << "mainWindow::save_queryTreeview: " << name << endl;
#endif
			m_settings.addQuery(queryProps);
		}
	}
}

//
// Populate menus
//
void mainWindow::populate_menus()
{
	if (m_pIndexMenu == NULL)
	{
		m_pIndexMenu = manage(new Menu());
		list1->set_submenu(*m_pIndexMenu);
	}
	else
	{
		// Clear the submenu
		m_pIndexMenu->items().clear();
	}

	SigC::Slot1<void, ustring> indexSlot = SigC::slot(*this, &mainWindow::on_index_changed);

	// Populate the submenu
	m_pIndexMenu->items().push_back(Menu_Helpers::MenuElem(_("My Documents")));
	MenuItem *pMenuItem = &m_pIndexMenu->items().back();
	// Bind the callback's parameter to the index name
	SigC::Slot0<void> daemonActivateSlot = sigc::bind(indexSlot, _("My Documents"));
	pMenuItem->signal_activate().connect(daemonActivateSlot);

	m_pIndexMenu->items().push_back(Menu_Helpers::MenuElem(_("My Web Pages")));
	pMenuItem = &m_pIndexMenu->items().back();
	// Bind the callback's parameter to the index name
	SigC::Slot0<void> documentsActivateSlot = sigc::bind(indexSlot, _("My Web Pages"));
	pMenuItem->signal_activate().connect(documentsActivateSlot);

	if (m_pCacheMenu == NULL)
	{
		m_pCacheMenu = manage(new Menu());
		viewcache1->set_submenu(*m_pCacheMenu);
	}
	else
	{
		// Clear the submenu
		m_pCacheMenu->items().clear();
	}

	SigC::Slot1<void, PinotSettings::CacheProvider> cacheSlot = SigC::slot(*this, &mainWindow::on_cache_changed);

	if (m_settings.m_cacheProviders.empty() == true)
	{
		// Hide the submenu
		viewcache1->hide();
	}
	else
	{
		// Populate the submenu
		for (vector<PinotSettings::CacheProvider>::iterator cacheIter = m_settings.m_cacheProviders.begin();
			cacheIter != m_settings.m_cacheProviders.end(); ++cacheIter)
		{
			m_pCacheMenu->items().push_back(Menu_Helpers::MenuElem(cacheIter->m_name));
			MenuItem *pMenuItem = &m_pCacheMenu->items().back();

			// Bind the callback's parameter to the cache provider
			SigC::Slot0<void> documentsActivateSlot = sigc::bind(cacheSlot, (*cacheIter));
			pMenuItem->signal_activate().connect(documentsActivateSlot);
		}
	}
}

//
// Query list selection changed
//
void mainWindow::on_queryTreeviewSelection_changed()
{
	TreeModel::iterator iter = queryTreeview->get_selection()->get_selected();
	// Anything selected ?
	if (iter)
	{
		// Enable those
		editQueryButton->set_sensitive(true);
		removeQueryButton->set_sensitive(true);
		findQueryButton->set_sensitive(true);
	}
	else
	{
		// Disable those
		editQueryButton->set_sensitive(false);
		removeQueryButton->set_sensitive(false);
		findQueryButton->set_sensitive(false);
	}
}

bool mainWindow::on_queryCompletion_match(const ustring &key, const TreeModel::const_iterator &iter)
{
	TreeModel::Row row = *iter;

	ustring match = row[m_liveQueryColumns.m_name];

	return true;
}

//
// Engines tree selection changed
//
void mainWindow::on_enginesTreeviewSelection_changed()
{
	list<TreeModel::Path> selectedEngines = m_pEnginesTree->getSelection();
	// If there are more than one row selected, don't bother
	if (selectedEngines.size() != 1)
	{
		return;
	}

	list<TreeModel::Path>::iterator enginePath = selectedEngines.begin();
	if (enginePath == selectedEngines.end())
	{
		return;
	}

	TreeModel::iterator engineIter = m_pEnginesTree->getIter(*enginePath);
	bool enableRemoveIndex = false;

	// Make sure it's a leaf node
	if (engineIter->children().empty() == true)
	{
		TreeModel::Row engineRow = *engineIter;

		// Is it an external index ?
		EnginesModelColumns &enginesColumns = m_pEnginesTree->getColumnRecord();
		EnginesModelColumns::EngineType engineType = engineRow[enginesColumns.m_type];
		if (engineType == EnginesModelColumns::INDEX_ENGINE)
		{
			// Yes, enable the remove index button
			enableRemoveIndex = true;
		}
	}
	removeIndexButton->set_sensitive(enableRemoveIndex);
}

//
// Results tree selection changed
//
void mainWindow::on_resultsTreeviewSelection_changed(ustring queryName)
{
	vector<DocumentInfo> resultsList;
	string url;
	bool hasSelection = false;

	NotebookPageBox *pNotebookPage = get_page(queryName, NotebookPageBox::RESULTS_PAGE);
	if (pNotebookPage != NULL)
	{
		ResultsPage *pResultsPage = dynamic_cast<ResultsPage*>(pNotebookPage);
		if (pResultsPage != NULL)
		{
			ResultsTree *pResultsTree = pResultsPage->getTree();
			if (pResultsTree != NULL)
			{
				hasSelection = pResultsTree->getSelection(resultsList);
			}
		}
	}

	if ((hasSelection == true) &&
		(resultsList.empty() == false))
	{
		bool firstResult = true, isViewable = true, isCached = false, isIndexed = false, isIndexable = true;

		for (vector<DocumentInfo>::iterator resultIter = resultsList.begin();
			resultIter != resultsList.end(); ++resultIter)
		{
			string url(resultIter->getLocation());
			Url urlObj(url);
			string protocol(urlObj.getProtocol());

#ifdef DEBUG
			cout << "mainWindow::on_resultsTreeviewSelection_changed: " << url << endl;
#endif
			if (firstResult == true)
			{
				// Show the URL of the first result in the status bar
				ustring statusText = _("Result location is");
				statusText += " ";
				statusText += url;
				set_status(statusText, true);
				firstResult = false;
			}

			// FIXME: there should be a way to know which protocols can be viewed/indexed
			if (protocol == "xapian")
			{
				isViewable = isCached = isIndexed = isIndexable = false;
				break;
			}
			else
			{
				if (m_settings.m_cacheProtocols.find(protocol) != m_settings.m_cacheProtocols.end())
				{
					// One document with a supported protocol is good enough
					isCached = true;
				}

				isIndexed = resultIter->getIsIndexed();
			}
		}

		// Enable these menu items
		viewresults1->set_sensitive(isViewable);
		viewcache1->set_sensitive(isCached);
		morelikethis1->set_sensitive(isIndexed);
		indexresults1->set_sensitive(isIndexable);
	}
	else
	{
		// Disable these menu items
		viewresults1->set_sensitive(false);
		viewcache1->set_sensitive(false);
		morelikethis1->set_sensitive(false);
		indexresults1->set_sensitive(false);

		// Reset
		set_status("");
	}
}

//
// Index tree selection changed
//
void mainWindow::on_indexTreeviewSelection_changed(ustring indexName)
{
	IndexPage *pIndexPage = dynamic_cast<IndexPage*>(get_page(indexName, NotebookPageBox::INDEX_PAGE));
	if (pIndexPage == NULL)
	{
		return;
	}

	IndexTree *pIndexTree = pIndexPage->getTree();
	if (pIndexTree != NULL)
	{
		string url(pIndexTree->getFirstSelectionURL());

		if (url.empty() == false)
		{
			bool isDocumentsIndex = true;

			// Enable these menu items unless it is not the documents index
			if (indexName != _("My Web Pages"))
			{
				isDocumentsIndex = false;
			}
			viewfromindex1->set_sensitive(true);
			refreshindex1->set_sensitive(isDocumentsIndex);
			unindex1->set_sensitive(isDocumentsIndex);
			showfromindex1->set_sensitive(true);

			// Show the URL in the status bar
			ustring statusText = _("Document location is");
			statusText += " ";
			statusText += pIndexTree->getFirstSelectionURL();
			set_status(statusText, true);
		}
	}
	else
	{
		// No, disable these
		viewfromindex1->set_sensitive(false);
		refreshindex1->set_sensitive(false);
		unindex1->set_sensitive(false);
		showfromindex1->set_sensitive(false);
	}
}

//
// Index > List Contents Of menu selected
//
void mainWindow::on_index_changed(ustring indexName)
{
	IndexTree *pIndexTree = NULL;
	IndexPage *pIndexPage = NULL;
	ustring labelName;
	bool foundPage = false;

#ifdef DEBUG
	cout << "mainWindow::on_index_changed: current index now " << indexName << endl;
#endif

	// Is there already a page for this index ?
	pIndexPage = dynamic_cast<IndexPage*>(get_page(indexName, NotebookPageBox::INDEX_PAGE));
	if (pIndexPage != NULL)
	{
		// Force a refresh
		pIndexTree = pIndexPage->getTree();
		if (pIndexTree != NULL)
		{
			pIndexTree->clear();
		}
		labelName = pIndexPage->getLabelName();
		foundPage = true;
	}

	if (foundPage == false)
	{
		NotebookTabBox *pTab = manage(new NotebookTabBox(indexName,
			NotebookPageBox::INDEX_PAGE));
		pTab->getCloseSignal().connect(
			SigC::slot(*this, &mainWindow::on_close_page));

		// Position the index tree
		pIndexTree = manage(new IndexTree(indexName, indexMenuitem->get_submenu(), m_settings));
		pIndexPage = manage(new IndexPage(indexName, pIndexTree, m_settings));
		// Connect to the "changed" signal
		pIndexTree->getSelectionChangedSignal().connect(
			SigC::slot(*this, &mainWindow::on_indexTreeviewSelection_changed));
		// Connect to the edit document signal
		pIndexTree->getEditDocumentSignal().connect(
			SigC::slot(*this, &mainWindow::on_showfromindex_activate));
		// Connect to the label changed signal
		pIndexPage->getLabelChangedSignal().connect(
			SigC::slot(*this, &mainWindow::on_label_changed));
		// ...and to the buttons clicked signals
		pIndexPage->getBackClickedSignal().connect(
			SigC::slot(*this, &mainWindow::on_indexBackButton_clicked));
		pIndexPage->getForwardClickedSignal().connect(
			SigC::slot(*this, &mainWindow::on_indexForwardButton_clicked));

		// Append the page
		if (m_state.write_lock_lists() == true)
		{
			int pageNum = m_pNotebook->append_page(*pIndexPage, *pTab);
			m_pNotebook->pages().back().set_tab_label_packing(false, true, Gtk::PACK_START);
			if (pageNum >= 0)
			{
				foundPage = true;
			}

			m_state.unlock_lists();
		}
	}

	if (foundPage == true)
	{
		browse_index(indexName, labelName, 0);
	}
}

//
// Results > View Cache menu selected
//
void mainWindow::on_cache_changed(PinotSettings::CacheProvider cacheProvider)
{
	if (cacheProvider.m_name.empty() == true)
	{
		return;
	}

	NotebookPageBox *pNotebookPage = get_current_page();
	if (pNotebookPage != NULL)
	{
		ResultsPage *pResultsPage = dynamic_cast<ResultsPage*>(pNotebookPage);
		if (pResultsPage != NULL)
		{
			ResultsTree *pResultsTree = pResultsPage->getTree();
			if (pResultsTree != NULL)
			{
				vector<DocumentInfo> resultsList;

				if (pResultsTree->getSelection(resultsList) == true)
				{
					for (vector<DocumentInfo>::iterator resultIter = resultsList.begin();
						resultIter != resultsList.end(); ++resultIter)
					{
						string url(resultIter->getLocation());
						Url urlObj(url);
						string protocol(urlObj.getProtocol());

						// Is this protocol supported ?
						if (cacheProvider.m_protocols.find(protocol) == cacheProvider.m_protocols.end())
						{
							// No, it isn't... This document will be open in place
							continue;
						}

						// Rewrite the URL
						ustring location(cacheProvider.m_location);
						ustring::size_type argPos = location.find("%url0");
						if (argPos != ustring::npos)
						{
							string::size_type pos = url.find("://");
							if ((pos != string::npos) &&
								(pos + 3 < url.length()))
							{
								// URL without protocol
								location.replace(argPos, 5, url.substr(pos + 3));
							}
						}
						else
						{
							argPos = location.find("%url");
							if (argPos != ustring::npos)
							{
								// Full protocol
								location.replace(argPos, 4, url);
							}
						}

						resultIter->setLocation(location);
#ifdef DEBUG
						cout << "mainWindow::on_cache_changed: rewritten "
							<< url << " to " << location << endl;
#endif
					}

					view_documents(resultsList);

					// Update the rows right now
					pResultsTree->setSelectionState(true);
				}
			}
		}
	}
}


//
// Index labels combo selection changed
//
void mainWindow::on_label_changed(ustring indexName, ustring labelName)
{
	IndexPage *pIndexPage = dynamic_cast<IndexPage*>(get_page(indexName, NotebookPageBox::INDEX_PAGE));
	if (pIndexPage == NULL)
	{
		return;
	}
	IndexTree *pIndexTree = pIndexPage->getTree();
	if (pIndexTree == NULL)
	{
		return;
	}

#ifdef DEBUG
	cout << "mainWindow::on_label_changed: called on " << labelName << endl;
#endif
	browse_index(indexName, labelName, 0);
}

//
// Notebook page switch
//
void mainWindow::on_switch_page(GtkNotebookPage *p0, guint p1)
{
	bool showResultsMenuitems = false;

	NotebookPageBox *pNotebookPage = dynamic_cast<NotebookPageBox*>(m_pNotebook->get_nth_page(p1));
	if (pNotebookPage != NULL)
	{
		NotebookPageBox::PageType type = pNotebookPage->getType();
		if (type == NotebookPageBox::RESULTS_PAGE)
		{
			// Show or hide the extract field
			ResultsPage *pResultsPage = dynamic_cast<ResultsPage*>(pNotebookPage);
			if (pResultsPage != NULL)
			{
				ResultsTree *pResultsTree = pResultsPage->getTree();
				if (pResultsTree != NULL)
				{
					// Sync the results tree with the menuitems
					pResultsTree->showExtract(showextract1->get_active());
					if (searchenginegroup1->get_active() == true)
					{
						pResultsTree->setGroupMode(ResultsTree::BY_ENGINE);
					}
					else
					{
						pResultsTree->setGroupMode(ResultsTree::BY_HOST);
					}
				}
			}

			showResultsMenuitems = true;
		}
#ifdef DEBUG
		cout << "mainWindow::on_switch_page: page " << p1 << " has type " << type << endl;
#endif
	}

	show_global_menuitems(showResultsMenuitems);
	// Did the page change ?
	if (m_state.m_currentPage != p1)
	{
		show_selectionbased_menuitems(false);
	}
	m_state.m_currentPage = p1;
}

//
// Notebook page closed
//
void mainWindow::on_close_page(ustring title, NotebookPageBox::PageType type)
{
	int pageDecrement = 0;

#ifdef DEBUG
	cout << "mainWindow::on_close_page: called for tab " << title << endl;
#endif
	int pageNum = get_page_number(title, type);
	if (pageNum >= 0)
	{
		if (m_state.write_lock_lists() == true)
		{
			// Remove the page
			m_pNotebook->remove_page(pageNum);

			m_state.unlock_lists();
		}
	}

	if (m_pNotebook->get_n_pages() - pageDecrement <= 0)
	{
		show_global_menuitems(false);
		show_selectionbased_menuitems(false);
	}

	// Reset
	set_status("");
}

//
// End of worker thread
//
void mainWindow::on_thread_end(WorkerThread *pThread)
{
	ustring status;
	string indexedUrl;

	if (pThread == NULL)
	{
		return;
	}
#ifdef DEBUG
	cout << "mainWindow::on_thread_end: end of thread " << pThread->getId() << endl;
#endif

	// What type of thread was it ?
	string type = pThread->getType();
	// Did the thread fail for some reason ?
	string threadStatus = pThread->getStatus();
	if (threadStatus.empty() == false)
	{
		// Yep, it did
		set_status(to_utf8(threadStatus));
		// Better reset that flag if an error occured while browsing an index
		if (type == "IndexBrowserThread")
		{
			m_state.m_browsingIndex = false;
		}
	}
	// Based on type, take the appropriate action...
	else if (type == "IndexBrowserThread")
	{
		char docsCountStr[64];

		IndexBrowserThread *pBrowseThread = dynamic_cast<IndexBrowserThread *>(pThread);
		if (pBrowseThread == NULL)
		{
			delete pThread;
			return;
		}

		IndexPage *pIndexPage = NULL;
		IndexTree *pIndexTree = NULL;
		ustring indexName = to_utf8(pBrowseThread->getIndexName());
		ustring labelName = to_utf8(pBrowseThread->getLabelName());

		// Find the page for this index
		// It may have been closed by the user
		pIndexPage = dynamic_cast<IndexPage*>(get_page(indexName, NotebookPageBox::INDEX_PAGE));
		if (pIndexPage != NULL)
		{
			pIndexTree = pIndexPage->getTree();
			if (pIndexTree != NULL)
			{
				const vector<DocumentInfo> &docsList = pBrowseThread->getDocuments();

				// Add the documents to the tree
				pIndexTree->addDocuments(docsList);

				pIndexPage->setDocumentsCount(pBrowseThread->getDocumentsCount());
				pIndexPage->updateButtonsState(m_maxDocsCount);

				status = _("Showing");
				status += " ";
				snprintf(docsCountStr, 64, "%u", pIndexTree->getRowsCount());
				status += docsCountStr;
				status += " ";
				status += _("of");
				status += " ";
				snprintf(docsCountStr, 64, "%u", pIndexPage->getDocumentsCount());
				status += docsCountStr;
				status += " ";
				status += _("documents, starting at");
				status += " ";
				snprintf(docsCountStr, 64, "%u", pIndexPage->getFirstDocument());
				status += docsCountStr;
				set_status(status);

				if (pIndexPage->getDocumentsCount() > 0)
				{
					// Refresh the tree
					pIndexTree->refresh();
				}
				m_state.m_browsingIndex = false;
			}
		}
	}
	else if (type == "QueryingThread")
	{
		std::map<string, TreeModel::iterator> updatedGroups;
		int pageNum = -1;

		QueryingThread *pQueryThread = dynamic_cast<QueryingThread *>(pThread);
		if (pQueryThread == NULL)
		{
			delete pThread;
			return;
		}

		QueryProperties queryProps = pQueryThread->getQuery();
		ustring queryName = to_utf8(queryProps.getName());
		ustring engineName = to_utf8(pQueryThread->getEngineName());
		string resultsCharset;
		const vector<DocumentInfo> &resultsList = pQueryThread->getResults(resultsCharset);

		status = _("Query");
		status += " ";
		status += queryName;
		status += " ";
		status += _("on");
		status += " ";
		status += engineName;
		status += " ";
		status += _("ended");
		set_status(status);

		// Find the page for this query
		ResultsTree *pResultsTree = NULL;
		ResultsPage *pResultsPage = dynamic_cast<ResultsPage*>(get_page(queryName, NotebookPageBox::RESULTS_PAGE));
		if (pResultsPage != NULL)
		{
			pResultsTree = pResultsPage->getTree();

			pageNum = get_page_number(queryName, NotebookPageBox::RESULTS_PAGE);
		}
		else
		{
			// There is none, create one
			NotebookTabBox *pTab = manage(new NotebookTabBox(queryName,
				NotebookPageBox::RESULTS_PAGE));
			pTab->getCloseSignal().connect(
				SigC::slot(*this, &mainWindow::on_close_page));

			ResultsTree::GroupByMode groupMode = ResultsTree::BY_ENGINE;
			if (searchenginegroup1->get_active() == false)
			{
				groupMode = ResultsTree::BY_HOST;
			}
			// Position the results tree
			pResultsTree = manage(new ResultsTree(queryName, resultsMenuitem->get_submenu(), groupMode, m_settings));
			pResultsPage = manage(new ResultsPage(queryName, pResultsTree, m_pNotebook->get_height(), m_settings));
			// Sync the results tree with the menuitems
			pResultsTree->showExtract(showextract1->get_active());
			// Connect to the "changed" signal
			pResultsTree->getSelectionChangedSignal().connect(
				SigC::slot(*this, &mainWindow::on_resultsTreeviewSelection_changed));
			// Connect to the view results signal
			pResultsTree->getViewResultsSignal().connect(
				SigC::slot(*this, &mainWindow::on_viewresults_activate));

			// Append the page
			if (m_state.write_lock_lists() == true)
			{
				pageNum = m_pNotebook->append_page(*pResultsPage, *pTab);
				m_pNotebook->pages().back().set_tab_label_packing(false, true, Gtk::PACK_START);
				// Switch to this new page
				m_pNotebook->set_current_page(pageNum);

				m_state.unlock_lists();
			}
		}

		if ((pageNum >= 0) &&
			(pResultsTree != NULL))
		{
			// Add the results to the tree, using the current group by mode
			pResultsTree->deleteResults(engineName);
			pResultsTree->addResults(engineName, resultsList,
				resultsCharset);
		}

		// Index results ?
		if ((queryProps.getIndexResults() == true) &&
			(resultsList.empty() == false))
		{
			set<string> labels;
			set<unsigned int> docsIds, daemonIds;
			string labelName(queryProps.getLabelName());

			// Set the list of labels
			if (labelName.empty() == false)
			{
				labels.insert(labelName);
			}

#ifdef DEBUG
			cout << "mainWindow::on_thread_end: indexing results, with label " << labelName << endl;
#endif
			for (vector<DocumentInfo>::const_iterator resultIter = resultsList.begin();
				resultIter != resultsList.end(); ++resultIter)
			{
				unsigned int indexId;
				unsigned int docId = resultIter->getIsIndexed(indexId);

				if (docId == 0)
				{
					DocumentInfo docInfo(resultIter->getTitle(), resultIter->getLocation(),
						resultIter->getType(), resultIter->getLanguage());

					// Set/reset labels
					docInfo.setLabels(labels);

					// This document is not in either index
					m_state.queue_index(docInfo);
				}
				else if (indexId == m_settings.getIndexId(_("My Web Pages")))
				{
					if (labelName.empty() == false)
					{
						docsIds.insert(docId);
					}
				}
				else if (indexId == m_settings.getIndexId(_("My Documents")))
				{
					if (labelName.empty() == false)
					{
						daemonIds.insert(docId);
					}
				}
			}

			if ((labelName.empty() == false) &&
				((docsIds.empty() == false) ||
				(daemonIds.empty() == false)))
			{
				// Apply the new label to existing documents
				start_thread(new LabelUpdateThread(labelName, docsIds, daemonIds));
			}
		}
	}
	else if (type == "ExpandQueryThread")
	{
		ExpandQueryThread *pExpandThread = dynamic_cast<ExpandQueryThread *>(pThread);
		if (pExpandThread == NULL)
		{
			delete pThread;
			return;
		}

		QueryProperties queryProps = pExpandThread->getQuery();
		const set<string> &expandTerms = pExpandThread->getExpandTerms();
		string queryName(_("More Like"));
		string moreLike;

		if (queryProps.getName().empty() == true)
		{
			queryName += "...";
		}
		else
		{
			queryName += " ";
			queryName += queryProps.getName();
		}
		queryProps.setName(queryName);

		if (queryProps.getFreeQuery().empty() == false)
		{
			moreLike = " ";
		}
		for (set<string>::const_iterator termIter = expandTerms.begin();
			termIter != expandTerms.end(); ++termIter)
		{
			if (moreLike.empty() == false)
			{
				moreLike += " ";
			}
			moreLike += *termIter;
		}

		// Does such a query already exist ?
		TreeModel::Children children = m_refQueryTree->children();
		for (TreeModel::Children::iterator iter = children.begin();
			iter != children.end(); ++iter)
		{
			TreeModel::Row row = *iter;

			if (queryName == from_utf8(row[m_queryColumns.m_name]))
			{
				m_settings.removeQuery(queryName);
				break;
			}
		}

		// Add these terms
		queryProps.setFreeQuery(queryProps.getFreeQuery() + moreLike);

		// Update everything
		if (m_settings.addQuery(queryProps) == true)
		{
			populate_queryTreeview(queryName);
			queryExpander->set_expanded(true);
		}
	}
	else if (type == "LabelUpdateThread")
	{
		LabelUpdateThread *pLabelUpdateThread = dynamic_cast<LabelUpdateThread *>(pThread);
		if (pLabelUpdateThread == NULL)
		{
			delete pThread;
			return;
		}

		status = _("Updated label(s)");
		set_status(status);
	}
	else if (type == "DownloadingThread")
	{
		DownloadingThread *pDownloadThread = dynamic_cast<DownloadingThread *>(pThread);
		if (pDownloadThread == NULL)
		{
			delete pThread;
			return;
		}

		string url = pDownloadThread->getURL();
		const Document *pDoc = pDownloadThread->getDocument();
		if (pDoc != NULL)
		{
			unsigned int dataLength = 0;
			const char *pData = pDoc->getData(dataLength);

			if ((pData != NULL) &&
				(dataLength > 0))
			{
				char inTemplate[21] = "/tmp/pinottempXXXXXX";
				int inFd = mkstemp(inTemplate);
				bool viewDoc = false;

				if (inFd != -1)
				{
					// Save the data into a temporary file
					if (write(inFd, (const void*)pData, dataLength) != -1)
					{
						viewDoc = true;
					}

					close(inFd);
				}

				if (viewDoc == true)
				{
					DocumentInfo docInfo(*pDoc);
					string fileName("file://");

					fileName += inTemplate;
					docInfo.setLocation(fileName);

					// View this document
					vector<DocumentInfo> documentsList;
					documentsList.push_back(docInfo);
					view_documents(documentsList);
					// FIXME: how do we know when to delete this document ?
				}
			}
		}
	}
	else if (type == "IndexingThread")
	{
		char docIdStr[64];

		IndexingThread *pIndexThread = dynamic_cast<IndexingThread *>(pThread);
		if (pIndexThread == NULL)
		{
			delete pThread;
			return;
		}

		// Get the URL we have just indexed
		indexedUrl = pIndexThread->getURL();

		// Get the document properties
		unsigned int docId = pIndexThread->getDocumentID();
		DocumentInfo docInfo = pIndexThread->getDocumentInfo();

		// Is the index still being shown ?
		IndexTree *pIndexTree = NULL;
		IndexPage *pIndexPage = dynamic_cast<IndexPage*>(get_page(_("My Web Pages"), NotebookPageBox::INDEX_PAGE));
		if (pIndexPage != NULL)
		{
			pIndexTree = pIndexPage->getTree();
		}

		// Did the thread perform an update ?
		if (pIndexThread->isNewDocument() == false)
		{
			// Yes, it did
			status = _("Updated document");
			status += " ";
			snprintf(docIdStr, 64, "%u", docId);
			status += docIdStr;

			if (pIndexTree != NULL)
			{
				// Update the index tree
				pIndexTree->updateDocumentInfo(docId, docInfo);
			}
		}
		else
		{
			status = _("Indexed");
			status += " ";
			status += to_utf8(indexedUrl);

			if (pIndexTree != NULL)
			{
				unsigned int rowsCount = pIndexTree->getRowsCount();

				// Ensure the last page is being displayed and is not full
				if ((pIndexPage->getFirstDocument() + rowsCount == pIndexPage->getDocumentsCount()) &&
					(rowsCount < m_maxDocsCount))
				{
					// Add a row to the index tree
					DocumentInfo indexedDoc(docInfo.getTitle(),
						docInfo.getLocation(), docInfo.getType(),
						docInfo.getLanguage());
					indexedDoc.setTimestamp(docInfo.getTimestamp());
					indexedDoc.setSize(docInfo.getSize());

					append_document(pIndexPage, _("My Web Pages"), indexedDoc);
				}
				pIndexPage->setDocumentsCount(pIndexPage->getDocumentsCount() + 1);
				pIndexPage->updateButtonsState(m_maxDocsCount);
			}

			// FIXME: update the result's indexed status in the results list !
		}

		set_status(status);
	}
	else if (type == "UnindexingThread")
	{
		UnindexingThread *pUnindexThread = dynamic_cast<UnindexingThread *>(pThread);
		if (pUnindexThread == NULL)
		{
			delete pThread;
			return;
		}

		if (pUnindexThread->getDocumentsCount() > 0)
		{
			ustring indexName(_("My Web Pages"));
			status = _("Unindexed document(s)");
			set_status(status);

			IndexPage *pIndexPage = dynamic_cast<IndexPage*>(get_page(indexName, NotebookPageBox::INDEX_PAGE));
			if (pIndexPage != NULL)
			{
				unsigned int docsCount = pIndexPage->getDocumentsCount();

				--docsCount;
				pIndexPage->setDocumentsCount(docsCount);
				pIndexPage->updateButtonsState(m_maxDocsCount);

				if ((docsCount >= pIndexPage->getFirstDocument() + m_maxDocsCount))
				{
					// Refresh this page
					browse_index(indexName, pIndexPage->getLabelName(), pIndexPage->getFirstDocument());
				}
			}
		}
		// Else, stay silent
	}
	else if (type == "UpdateDocumentThread")
	{
		UpdateDocumentThread *pUpdateThread = dynamic_cast<UpdateDocumentThread *>(pThread);
		if (pUpdateThread == NULL)
		{
			delete pThread;
			return;
		}

		IndexPage *pIndexPage = dynamic_cast<IndexPage*>(get_page(_("My Web Pages"), NotebookPageBox::INDEX_PAGE));
		if (pIndexPage != NULL)
		{
			IndexTree *pIndexTree = pIndexPage->getTree();
			if (pIndexTree != NULL)
			{
				pIndexTree->updateDocumentInfo(pUpdateThread->getDocumentID(),
					pUpdateThread->getDocumentInfo());
			}
		}

		status = _("Updated document properties");
		set_status(status);
	}
	else if (type == "StartDaemonThread")
	{
#ifdef DEBUG
		cout << "mainWindow::on_thread_end: started daemon" << endl;
#endif
	}

	// Delete the thread
	delete pThread;;

	// We might be able to run a queued action
	m_state.pop_queue(indexedUrl);

	// Any threads left to return ?
	if (m_state.get_threads_count() == 0)
	{
		if (m_timeoutConnection.connected() == true)
		{
			m_timeoutConnection.block();
			m_timeoutConnection.disconnect();
			mainProgressbar->set_fraction(0.0);
		}
	}
}

//
// Message reception by EnginesTree
//
void mainWindow::on_editindex(ustring indexName, ustring location)
{
#ifdef DEBUG
	cout << "mainWindow::on_editindex: called" << endl;
#endif
	// Open the index properties dialog
	indexDialog indexBox(indexName, location);
	indexBox.show();
	if (indexBox.run() != RESPONSE_OK)
	{
		return;
	}

	if ((indexName != indexBox.getName()) ||
		(location != indexBox.getLocation()))
	{
		ustring newName = indexBox.getName();
		ustring newLocation = indexBox.getLocation();

		// Is the name okay ?
		if (indexBox.badName() == true)
		{
			ustring statusText = _("Couldn't rename index, name");
			statusText += " ";
			statusText += newName;
			statusText += " ";
			statusText +=  _("is already in use");

			// Tell user name is bad
			set_status(statusText);
			return;
		}

		// The only way to edit an index right now is to remove it
		// first, then add it again
		if ((m_settings.removeIndex(from_utf8(indexName)) == false) ||
			(m_settings.addIndex(from_utf8(newName),
				from_utf8(newLocation)) == false))
		{
			ustring statusText = _("Couldn't rename index");
			statusText += " ";
			statusText += indexName;

			// An error occured
			set_status(statusText);
			return;
		}

		// Refresh the engines list
		m_pEnginesTree->populate();
	}

	set_status(_("Edited index"));
}

//
// Session > Statistics menu selected
//
void mainWindow::on_statistics_activate()
{
	statisticsDialog statsDialog;
	statsDialog.show();
	statsDialog.run();
}

//
// Edit > Preferences menu selected
//
void mainWindow::on_configure_activate()
{
	prefsDialog prefsBox;
	prefsBox.show();
	if (prefsBox.run() != RESPONSE_OK)
	{
		return;
	}
#ifdef DEBUG
	cout << "mainWindow::on_configure_activate: settings changed" << endl;
#endif

	if (m_state.read_lock_lists() == true)
	{
		for (int pageNum = 0; pageNum < m_pNotebook->get_n_pages(); ++pageNum)
		{
			Widget *pPage = m_pNotebook->get_nth_page(pageNum);
			if (pPage != NULL)
			{
				IndexPage *pIndexPage = dynamic_cast<IndexPage*>(pPage);
				if (pIndexPage != NULL)
				{
					// Synchronize the labels list with the new settings
					pIndexPage->populateLabelCombobox();

					NotebookPageBox *pNotebookPage = dynamic_cast<NotebookPageBox*>(pPage);
					if (pNotebookPage != NULL)
					{
						// The current label may have changed, refresh the list
						browse_index(pNotebookPage->getTitle(), pIndexPage->getLabelName(), 0, false);
					}
				}
			}
		}

		m_state.unlock_lists();
	}

	// Is starting the daemon necessary ?
	if (prefsBox.startDaemon() == true)
	{
		start_thread(new StartDaemonThread());
	}

	// Any labels to delete or rename ?
	const set<string> &labelsToDelete = prefsBox.getLabelsToDelete();
	const std::map<string, string> &labelsToRename = prefsBox.getLabelsToRename();
	if ((labelsToDelete.empty() == false) ||
		(labelsToRename.empty() == false))
	{
		start_thread(new LabelUpdateThread(labelsToDelete, labelsToRename));
	}
}

//
// Session > Quit menu selected
//
void mainWindow::on_quit_activate()
{
	on_mainWindow_delete_event(NULL);
}

//
// Edit > Cut menu selected
//
void mainWindow::on_cut_activate()
{
	// Copy
	on_copy_activate();
	// ...and delete
	on_delete_activate();
}

//
// Edit > Copy menu selected
//
void mainWindow::on_copy_activate()
{
	ustring text;

	if (liveQueryEntry->is_focus() == true)
	{
		liveQueryEntry->copy_clipboard();
		return;
	}
	else if (queryTreeview->is_focus() == true)
	{
#ifdef DEBUG
		cout << "mainWindow::on_copy_activate: query tree" << endl;
#endif
		TreeModel::iterator iter = queryTreeview->get_selection()->get_selected();
		TreeModel::Row row = *iter;
		// Copy only the query name, not the summary
		text = row[m_queryColumns.m_name];
	}
	else
	{
		// The focus may be on one of the notebook pages
		NotebookPageBox *pNotebookPage = get_current_page();
		if (pNotebookPage != NULL)
		{
			vector<DocumentInfo> documentsList;
			bool firstItem = true;

			ResultsPage *pResultsPage = dynamic_cast<ResultsPage*>(pNotebookPage);
			if (pResultsPage != NULL)
			{
#ifdef DEBUG
				cout << "mainWindow::on_copy_activate: results tree" << endl;
#endif
				ResultsTree *pResultsTree = pResultsPage->getTree();
				if (pResultsTree != NULL)
				{
					TreeView *pTree = pResultsTree->getExtractTree();
					if ((pTree != NULL) &&
						(pTree->is_focus() == true))
					{
						// Retrieve the extract
						text = pResultsTree->getExtract();
					}
					else
					{
						// Get the current results selection
						pResultsTree->getSelection(documentsList);
					}
				}
			}

			IndexPage *pIndexPage = dynamic_cast<IndexPage*>(pNotebookPage);
			if (pIndexPage != NULL)
			{
#ifdef DEBUG
				cout << "mainWindow::on_copy_activate: index tree" << endl;
#endif
				IndexTree *pIndexTree = pIndexPage->getTree();
				if (pIndexTree != NULL)
				{
					// Get the current documents selection
					pIndexTree->getSelection(documentsList);
				}
			}

			for (vector<DocumentInfo>::const_iterator docIter = documentsList.begin();
				docIter != documentsList.end(); ++docIter)
			{
				string location(docIter->getLocation());

				if (firstItem == false)
				{
					text += "\n";
				}
				text += docIter->getTitle();
				if (location.empty() == false)
				{
					text += " ";
					text += location;
				}
				firstItem = false;
			}
		}

		if (text.empty() == true)
		{
			// Only rows from the query, results and index trees can be copied
#ifdef DEBUG
			cout << "mainWindow::on_copy_activate: other" << endl;
#endif
			return;
		}
	}
	
	RefPtr<Clipboard> refClipboard = Clipboard::get();
	if (refClipboard)
	{
		refClipboard->set_text(text);
	}
}

//
// Edit > Paste menu selected
//
void mainWindow::on_paste_activate()
{
	RefPtr<Clipboard> refClipboard = Clipboard::get();

	if (!refClipboard)
	{
		return;
	}

	if (refClipboard->wait_is_text_available() == false)
	{
		return;
	}

	ustring clipText = refClipboard->wait_for_text();
	if (liveQueryEntry->is_focus() == true)
	{
		int currentPosition = liveQueryEntry->get_position();

		// Paste where the cursor is
		liveQueryEntry->insert_text(clipText, clipText.length(), currentPosition);
	}
	else if (queryTreeview->is_focus() == true)
	{
#ifdef DEBUG
		cout << "mainWindow::on_paste_activate: query tree" << endl;
#endif
		// Use whatever text is in the clipboard as query name
		// FIXME: look for \n as query fields separators ?
		QueryProperties queryProps = QueryProperties(from_utf8(clipText), from_utf8(clipText));
		edit_query(queryProps, true);
	}
	else
	{
		// Only the query tree can be pasted into, others are read-only
#ifdef DEBUG
		cout << "mainWindow::on_paste_activate: other" << endl;
#endif
		return;
	}
}

//
// Edit > Delete menu selected
//
void mainWindow::on_delete_activate()
{
	if (liveQueryEntry->is_focus() == true)
	{
		liveQueryEntry->delete_selection();
	}
	else
	{
		NotebookPageBox *pNotebookPage = get_current_page();
		if (pNotebookPage != NULL)
		{
			ResultsPage *pResultsPage = dynamic_cast<ResultsPage*>(pNotebookPage);
			if (pResultsPage != NULL)
			{
#ifdef DEBUG
				cout << "mainWindow::on_delete_activate: results tree" << endl;
#endif
				ResultsTree *pResultsTree = pResultsPage->getTree();
				if (pResultsTree != NULL)
				{
					pResultsTree->deleteSelection();
				}
			}
		}
	}
	// Nothing else can be deleted
}

//
// Results > Clear menu selected
//
void mainWindow::on_clearresults_activate()
{
	NotebookPageBox *pNotebookPage = get_current_page();
	if (pNotebookPage != NULL)
	{
		ResultsPage *pResultsPage = dynamic_cast<ResultsPage*>(pNotebookPage);
		if (pResultsPage != NULL)
		{
			ResultsTree *pResultsTree = pResultsPage->getTree();
			if (pResultsTree != NULL)
			{
				pResultsTree->clear();
			}
		}
	}
}

//
// Results > Show Extract menu selected
//
void mainWindow::on_showextract_activate()
{
	NotebookPageBox *pNotebookPage = get_current_page();
	if (pNotebookPage != NULL)
	{
		ResultsPage *pResultsPage = dynamic_cast<ResultsPage*>(pNotebookPage);
		if (pResultsPage != NULL)
		{
			ResultsTree *pResultsTree = pResultsPage->getTree();
			if (pResultsTree != NULL)
			{
				pResultsTree->showExtract(showextract1->get_active());
			}
		}
	}
}

//
// Results > Group menu selected
//
void mainWindow::on_groupresults_activate()
{
	// What's the new grouping criteria ?
	NotebookPageBox *pNotebookPage = get_current_page();
	if (pNotebookPage != NULL)
	{
		ResultsPage *pResultsPage = dynamic_cast<ResultsPage*>(pNotebookPage);
		if (pResultsPage != NULL)
		{
			ResultsTree *pResultsTree = pResultsPage->getTree();
			if (pResultsTree != NULL)
			{
				if (searchenginegroup1->get_active() == true)
				{
					pResultsTree->setGroupMode(ResultsTree::BY_ENGINE);
				}
				else
				{
					pResultsTree->setGroupMode(ResultsTree::BY_HOST);
				}
			}
		}
	}
}

//
// Results > View menu selected
//
void mainWindow::on_viewresults_activate()
{
	NotebookPageBox *pNotebookPage = get_current_page();
	if (pNotebookPage != NULL)
	{
		ResultsPage *pResultsPage = dynamic_cast<ResultsPage*>(pNotebookPage);
		if (pResultsPage != NULL)
		{
			ResultsTree *pResultsTree = pResultsPage->getTree();
			if (pResultsTree != NULL)
			{
				vector<DocumentInfo> resultsList;

				if (pResultsTree->getSelection(resultsList) == true)
				{
					view_documents(resultsList);

					// We can update the rows right now
					pResultsTree->setSelectionState(true);
				}
			}
		}
	}
}

//
// Results > More Like This menu selected
//
void mainWindow::on_morelikethis_activate()
{
	QueryProperties queryProps;
	vector<DocumentInfo> resultsList;
	string queryName;

	NotebookPageBox *pNotebookPage = get_current_page();
	if (pNotebookPage != NULL)
	{
		ResultsPage *pResultsPage = dynamic_cast<ResultsPage*>(pNotebookPage);
		if (pResultsPage != NULL)
		{
			ResultsTree *pResultsTree = pResultsPage->getTree();
			if (pResultsTree != NULL)
			{
				pResultsTree->getSelection(resultsList);
			}

			queryName = from_utf8(pResultsPage->getTitle());
		}
	}

	// Find this query
	if (queryName == _("Live query"))
	{
		queryProps.setName(queryName);
		queryProps.setFreeQuery(from_utf8(liveQueryEntry->get_text()));
	}
	else
	{
		bool foundQuery = false;

		TreeModel::Children children = m_refQueryTree->children();
		for (TreeModel::Children::iterator iter = children.begin();
			iter != children.end(); ++iter)
		{
			TreeModel::Row row = *iter;

			if (queryName == from_utf8(row[m_queryColumns.m_name]))
			{
				queryProps = row[m_queryColumns.m_properties];
				foundQuery = true;
				break;
			}
		}

		// Maybe the query was deleted
		if (foundQuery == false)
		{
			ustring status(_("Couldn't find query"));
			status += " ";
			status += queryName;
			set_status(status);
			return;
		}
	}

	set<string> locations;
	for (vector<DocumentInfo>::const_iterator docIter = resultsList.begin();
		docIter != resultsList.end(); ++docIter)
	{
		if (docIter->getIsIndexed() == true)
		{
			locations.insert(docIter->getLocation());
		}
	}

	if (locations.empty() == false)
	{
		// Spawn a new thread
		start_thread(new ExpandQueryThread(queryProps, locations));
	}
}

//
// Results > Index menu selected
//
void mainWindow::on_indexresults_activate()
{
	vector<DocumentInfo> resultsList;

	NotebookPageBox *pNotebookPage = get_current_page();
	if (pNotebookPage != NULL)
	{
		ResultsPage *pResultsPage = dynamic_cast<ResultsPage*>(pNotebookPage);
		if (pResultsPage != NULL)
		{
			ResultsTree *pResultsTree = pResultsPage->getTree();
			if (pResultsTree != NULL)
			{
				pResultsTree->getSelection(resultsList, true);

				// Go through selected results
				for (vector<DocumentInfo>::const_iterator resultIter = resultsList.begin();
					resultIter != resultsList.end(); ++resultIter)
				{
#ifdef DEBUG
					cout << "mainWindow::on_indexresults_activate: URL is " << resultIter->getLocation() << endl;
#endif
					ustring status = m_state.queue_index(*resultIter);
					if (status.empty() == false)
					{
						set_status(status);
					}
				}
			}
		}
	}
}

//
// Index > Import menu selected
//
void mainWindow::on_import_activate()
{
	m_state.disconnect();

	importDialog importBox;
	importBox.show();
	importBox.run();

	m_state.connect();

	// Was anything imported ?
	if (importBox.getDocumentsCount() > 0)
	{
		ustring indexName(_("My Web Pages"));

		// Is the index still being shown ?
		IndexPage *pIndexPage = dynamic_cast<IndexPage*>(get_page(indexName, NotebookPageBox::INDEX_PAGE));
		if (pIndexPage != NULL)
		{
			// Refresh
			browse_index(indexName, pIndexPage->getLabelName(), 0);
		}
	}
}

//
// Index > View menu selected
//
void mainWindow::on_viewfromindex_activate()
{
	IndexPage *pIndexPage = dynamic_cast<IndexPage*>(get_current_page());
	if (pIndexPage != NULL)
	{
		IndexTree *pIndexTree = pIndexPage->getTree();

		if (pIndexTree != NULL)
		{
			vector<DocumentInfo> documentsList;

			if (pIndexTree->getSelection(documentsList) == true)
			{
				view_documents(documentsList);
			}
		}
	}
}

//
// Index > Refresh menu selected
//
void mainWindow::on_refreshindex_activate()
{
	vector<DocumentInfo> documentsList;

	// Get the current documents selection
	IndexPage *pIndexPage = dynamic_cast<IndexPage*>(get_current_page());
	if (pIndexPage != NULL)
	{
		IndexTree *pIndexTree = pIndexPage->getTree();

		if (pIndexTree != NULL)
		{
			if ((pIndexTree->getSelection(documentsList) == false) ||
				(documentsList.empty() == true))
			{
				// No selection
				return;
			}
		}
	}

	for (vector<DocumentInfo>::const_iterator docIter = documentsList.begin();
		docIter != documentsList.end(); ++docIter)
	{
		unsigned int indexId = 0;
		unsigned int docId = docIter->getIsIndexed(indexId);

		if (docId == 0)
		{
			continue;
		}
#ifdef DEBUG
		cout << "mainWindow::on_refreshindex_activate: URL is " << docIter->getLocation() << endl;
#endif

		// Add this action to the queue
		DocumentInfo docInfo(docIter->getTitle(), docIter->getLocation(), 
			docIter->getType(), docIter->getLanguage());
		docInfo.setTimestamp(docIter->getTimestamp());
		docInfo.setSize(docIter->getSize());

		ustring status = m_state.queue_index(docInfo);
		if (status.empty() == false)
		{
			set_status(status);
		}
	}
}

//
// Index > Show Properties menu selected
//
void mainWindow::on_showfromindex_activate()
{
	vector<DocumentInfo> documentsList;
	set<string> docLabels;
	string indexName, labelName;
	DocumentInfo docInfo;
	unsigned int docId = 0, termsCount = 0;
	int width, height;
	bool matchedLabel = false, editDocument = false;

#ifdef DEBUG
	cout << "mainWindow::on_showfromindex_activate: called" << endl;
#endif
	IndexTree *pIndexTree = NULL;
	IndexPage *pIndexPage = dynamic_cast<IndexPage*>(get_current_page());
	if (pIndexPage != NULL)
	{
		indexName = from_utf8(pIndexPage->getTitle());
		labelName = from_utf8(pIndexPage->getLabelName());
		pIndexTree = pIndexPage->getTree();
	}

	const std::map<string, string> &indexesMap = m_settings.getIndexes();
	std::map<string, string>::const_iterator mapIter = indexesMap.find(indexName);	
	if (mapIter == indexesMap.end())
	{
		ustring statusText = _("Index");
		statusText += " ";
		statusText += indexName;
		statusText += " ";
		statusText += _("doesn't exist");
		set_status(statusText);
		return;
	}

	// Get the current documents selection
	if ((pIndexTree == NULL) ||
		(pIndexTree->getSelection(documentsList) == false) ||
		(documentsList.empty() == true))
	{
		// No selection
		return;
	}

	IndexInterface *pIndex = m_settings.getIndex(mapIter->second);
	if ((pIndex == NULL) ||
		(pIndex->isGood() == false))
	{
		if (pIndex != NULL)
		{
			delete pIndex;
		}
		return;
	}

	// If there's only one document selected, get its labels
	if (documentsList.size() == 1)
	{
		vector<DocumentInfo>::iterator docIter = documentsList.begin();
		unsigned int indexId = 0;

		// Get the document ID
		docId = docIter->getIsIndexed(indexId);

		// Get the properties from the index because they have been altered
		// by the tree for display purposes
		pIndex->getDocumentInfo(docId, docInfo);
		pIndex->getDocumentLabels(docId, docLabels);
		termsCount = pIndex->getDocumentTermsCount(docId);

		// Does it match the current label ?
		if ((labelName.empty() == false) &&
			(find(docLabels.begin(), docLabels.end(), labelName) != docLabels.end()))
		{
			matchedLabel = true;
		}

		editDocument = true;
	}
	else
	{
		// If all documents are of the same language, show it
		for (vector<DocumentInfo>::iterator docIter = documentsList.begin();
			docIter != documentsList.end(); ++docIter)
		{
			if (docInfo.getLanguage().empty() == true)
			{
				docInfo.setLanguage(docIter->getLanguage());
			}
			if (docIter->getLanguage() != docInfo.getLanguage())
			{
				// They aren't
				docInfo.setLanguage("");
				break;
			}
		}

		// Show a blank labels list
	}

	// Let the user set the labels
	get_size(width, height);
	propertiesDialog propertiesBox(docLabels, docInfo, termsCount, editDocument);
	propertiesBox.setHeight(height / 2);
	propertiesBox.show();
	if (propertiesBox.run() != RESPONSE_OK)
	{
		delete pIndex;
		return;
	}

	const set<string> &labels = propertiesBox.getLabels();
	string newTitle(propertiesBox.getDocumentInfo().getTitle());
	string newLanguage(propertiesBox.getDocumentInfo().getLanguage());
#ifdef DEBUG
	cout << "mainWindow::on_showfromindex_activate: properties changed to "
		<< newTitle << ", " << newLanguage << endl;
#endif

	// Now apply these labels to all the documents
	if ((docLabels.empty() == false) ||
		(labels.empty() == false) ||
		(documentsList.size() > 1))
	{
		set<unsigned int> docIds;

		for (vector<DocumentInfo>::iterator docIter = documentsList.begin();
			docIter != documentsList.end(); ++docIter)
		{
			unsigned int indexId = 0;

			docIds.insert(docIter->getIsIndexed(indexId));
		}

		// Set the document's labels list
		pIndex->setDocumentsLabels(docIds, labels);
	}

	if ((documentsList.size() == 1) &&
		(docId > 0))
	{
		// Did the title or language change ?
		if ((newTitle != docInfo.getTitle()) ||
			(newLanguage != docInfo.getLanguage()))
		{
			docInfo.setTitle(newTitle);
			docInfo.setLanguage(newLanguage);

			// Update the document
			start_thread(new UpdateDocumentThread(indexName, docId, docInfo));
		}
	}
	else
	{
		// Was the language changed ?
		if (newLanguage.empty() == false)
		{
			// Update all documents
			for (vector<DocumentInfo>::iterator docIter = documentsList.begin();
				docIter != documentsList.end(); ++docIter)
			{
				unsigned int indexId = 0;
				unsigned int docId = docIter->getIsIndexed(indexId);

				docIter->setLanguage(newLanguage);

				start_thread(new UpdateDocumentThread(indexName, docId, *docIter));
			}
		}

		if (labelName.empty() == false)
		{
			// The current label may have been applied to or removed from
			// one or more of the selected documents, so refresh the list
			browse_index(indexName, labelName, 0, 0);
		}
	}

	delete pIndex;
}

//
// Index > Unindex menu selected
//
void mainWindow::on_unindex_activate()
{
	vector<DocumentInfo> documentsList;
	ustring boxTitle = _("Remove this document from the index ?");

	IndexTree *pIndexTree = NULL;
	IndexPage *pIndexPage = dynamic_cast<IndexPage*>(get_current_page());
	if (pIndexPage != NULL)
	{
		pIndexTree = pIndexPage->getTree();
	}

	// Get the current documents selection
	if ((pIndexTree == NULL) ||
		(pIndexTree->getSelection(documentsList) == false) ||
		(documentsList.empty() == true))
	{
		return;
	}

	if (documentsList.size() > 1)
	{
		boxTitle = _("Remove these documents from the index ?");
	}

	// Ask for confirmation
	MessageDialog msgDialog(boxTitle, false, MESSAGE_QUESTION, BUTTONS_YES_NO);
	msgDialog.set_transient_for(*this);
	msgDialog.show();
	int result = msgDialog.run();
	if (result == RESPONSE_NO)
	{
		return;
	}

	// Remove these documents from the tree
	pIndexTree->deleteSelection();

	set<unsigned int> docIdList;
	for (vector<DocumentInfo>::const_iterator docIter = documentsList.begin();
		docIter != documentsList.end(); ++docIter)
	{
		unsigned int indexId = 0;
		unsigned int docId = docIter->getIsIndexed(indexId);

		if (docId > 0)
		{
			docIdList.insert(docId);
		}
	}

	if (docIdList.empty() == false)
	{
		// Queue this action
#ifdef DEBUG
		cout << "mainWindow::on_unindex_activate: " << docIdList.size() << " documents to unindex" << endl;
#endif
		start_thread(new UnindexingThread(docIdList));
	}
}

//
// Help > About menu selected
//
void mainWindow::on_about_activate()
{
	AboutDialog aboutBox;

	aboutBox.set_comments(_("A metasearch tool for the Free Desktop") + string(".\n") +
		_("Search the Web and your documents !"));
	aboutBox.set_copyright(_("(C) 2005 Fabrice Colin"));
	aboutBox.set_name("Pinot");
	aboutBox.set_version(VERSION);
	aboutBox.set_website("http://pinot.berlios.de/");
	aboutBox.set_website_label("http://pinot.berlios.de/");
	aboutBox.show();
	aboutBox.run();
}

//
// Activity timeout elapsed
//
bool mainWindow::on_activity_timeout()
{
	if (m_timeoutConnection.blocked() == false)
	{
		mainProgressbar->pulse();
	}
#ifdef DEBUG
	else cout << "mainWindow::on_activity_timeout: blocked" << endl;
#endif

	return true;
}

//
// Add index button click
//
void mainWindow::on_addIndexButton_clicked()
{
	// Open the index properties dialog
	indexDialog indexBox;
	indexBox.show();
	if (indexBox.run() != RESPONSE_OK)
	{
		return;
	}

	ustring name = indexBox.getName();
	ustring location = indexBox.getLocation();

	// Is the name okay ?
	if (indexBox.badName() == true)
	{
		ustring statusText = _("Index name");
		statusText += " ";
		statusText += name;
		statusText += " ";
		statusText +=  _("is already in use");

		// Tell user name is bad
		set_status(statusText);
		return;
	}

	// Add the new index
	if (m_settings.addIndex(from_utf8(name),
			from_utf8(location)) == false)
	{
		ustring statusText = _("Couldn't add index");
		statusText += " ";
		statusText += name;

		// An error occured
		set_status(statusText);
	}
	else
	{
		// Refresh the indexes list
		removeIndexButton->set_sensitive(false);
		m_pEnginesTree->populate();
	}

	set_status(_("Added new index"));
}

//
// Remove index button click
//
void mainWindow::on_removeIndexButton_clicked()
{
	list<TreeModel::Path> selectedEngines = m_pEnginesTree->getSelection();
	// If there are more than one row selected, don't bother
	if (selectedEngines.size() != 1)
	{
		return;
	}

	list<TreeModel::Path>::iterator enginePath = selectedEngines.begin();
	if (enginePath == selectedEngines.end())
	{
		return;
	}

	TreeModel::iterator engineIter = m_pEnginesTree->getIter(*enginePath);
	TreeModel::Row engineRow = *engineIter;

	// Make sure the engine is an external index
	EnginesModelColumns &engineColumns = m_pEnginesTree->getColumnRecord();
	EnginesModelColumns::EngineType engineType = engineRow[engineColumns.m_type];
	if (engineType == EnginesModelColumns::INDEX_ENGINE)
	{
		ustring name = engineRow[engineColumns.m_name];

		// Remove it
		// FIXME: ask for confirmation ?
		if (m_settings.removeIndex(from_utf8(name)) == false)
		{
			ustring statusText = _("Couldn't remove index");
			statusText += " ";
			statusText += name;

			// An error occured
			set_status(statusText);
		}
		else
		{
			// Refresh the indexes list
			removeIndexButton->set_sensitive(false);
			m_pEnginesTree->populate();
		}
	}
}

//
// Show or hide the engines list
//
void mainWindow::on_enginesTogglebutton_toggled()
{
	if (enginesTogglebutton->get_active() == true)
	{
		leftVbox->show();
	}
	else
	{
		leftVbox->hide();
	}
}

//
// Live query entry change
//
void mainWindow::on_liveQueryEntry_changed()
{
	if (m_settings.m_suggestQueryTerms == false)
	{
		// This functionality is disabled
		return;
	}

	ustring prefix, term = liveQueryEntry->get_text();
	unsigned int liveQueryLength = term.length();

	// Reset the list
	m_refLiveQueryList->clear();
	// Get the last term from the entry field
	ustring::size_type pos = term.find_last_of(" ");
	if (pos != ustring::npos)
	{
		ustring liveQuery(term);

		prefix = liveQuery.substr(0, pos);
		prefix += " ";
		term = liveQuery.substr(pos + 1);
	}

	// If characters are being deleted or if the term is too short, don't
	// attempt to offer suggestions
	if ((m_state.m_liveQueryLength > liveQueryLength) ||
		(term.empty() == true) ||
		(m_refLiveQueryCompletion->get_minimum_key_length() > term.length()))
	{
		m_state.m_liveQueryLength = liveQueryLength;
		return;
	}
	m_state.m_liveQueryLength = liveQueryLength;

	// Query the merged index
	IndexInterface *pIndex = m_settings.getIndex("MERGED");
	if (pIndex != NULL)
	{
		set<string> suggestedTerms;
		int termIndex = 0;

		// Get a list of suggestions
		pIndex->getCloseTerms(from_utf8(term), suggestedTerms);

		// Populate the list
		for (set<string>::iterator termIter = suggestedTerms.begin();
			termIter != suggestedTerms.end(); ++termIter)
		{
			TreeModel::iterator iter = m_refLiveQueryList->append();
			TreeModel::Row row = *iter;

			row[m_liveQueryColumns.m_name] = prefix + to_utf8(*termIter);
			++termIndex;
		}
#ifdef DEBUG
		cout << "mainWindow::on_liveQueryEntry_changed: " << termIndex << " suggestions" << endl;
#endif

		delete pIndex;
	}
}

//
// Return key pressed in live query entry field
//
void mainWindow::on_liveQueryEntry_activate()
{
	on_findButton_clicked();
}

//
// Find button click
//
void mainWindow::on_findButton_clicked()
{
	QueryProperties queryProps(_("Live query"), from_utf8(liveQueryEntry->get_text()));

	run_search(queryProps);
}

//
// Add query button click
//
void mainWindow::on_addQueryButton_clicked()
{
	// Even though live queries terms are now ANDed together,
	// use them as OR terms when creating a new stored query
	QueryProperties queryProps = QueryProperties("", from_utf8(liveQueryEntry->get_text()));

	edit_query(queryProps, true);
}

//
// Edit query button click
//
void mainWindow::on_editQueryButton_clicked()
{
	TreeModel::iterator iter = queryTreeview->get_selection()->get_selected();
	// Anything selected ?
	if (iter)
	{
		TreeModel::Row row = *iter;
#ifdef DEBUG
		cout << "mainWindow::on_editQueryButton_clicked: selected " << row[m_queryColumns.m_name] << endl;
#endif

		// Edit this query's properties
		QueryProperties queryProps = row[m_queryColumns.m_properties];
		edit_query(queryProps, false);
	}
}

//
// Remove query button click
//
void mainWindow::on_removeQueryButton_clicked()
{
	TreeModel::iterator iter = queryTreeview->get_selection()->get_selected();
	// Anything selected ?
	if (iter)
	{
		TreeModel::Row row = *iter;
		string queryName = from_utf8(row[m_queryColumns.m_name]);

		if (m_settings.removeQuery(queryName) == true)
		{
			// Remove records from QueryHistory
			QueryHistory history(m_settings.m_historyDatabase);
			history.deleteItems(queryName, true);

			// Select another row
			queryTreeview->get_selection()->unselect(iter);
			TreeModel::Path path = m_refQueryTree->get_path(iter);
			path.next();
			queryTreeview->get_selection()->select(path);
			// Erase
			m_refQueryTree->erase(row);

			queryTreeview->columns_autosize();
		}
	}
}

//
// Find query button click
//
void mainWindow::on_findQueryButton_clicked()
{
	TreeModel::iterator queryIter = queryTreeview->get_selection()->get_selected();
	// Anything selected ?
	if (queryIter)
	{
		TreeModel::Row queryRow = *queryIter;
		time_t lastRunTime = time(NULL);

		QueryProperties queryProps = queryRow[m_queryColumns.m_properties];
		run_search(queryProps);

		// Update the Last Run column
		queryRow[m_queryColumns.m_lastRun] = to_utf8(TimeConverter::toTimestamp(lastRunTime));
		queryRow[m_queryColumns.m_lastRunTime] = lastRunTime;
	}
}

//
// Index back button click
//
void mainWindow::on_indexBackButton_clicked(ustring indexName)
{
	IndexPage *pIndexPage = dynamic_cast<IndexPage*>(get_page(indexName, NotebookPageBox::INDEX_PAGE));
	if (pIndexPage != NULL)
	{
		if (pIndexPage->getFirstDocument() >= m_maxDocsCount)
		{
			browse_index(indexName, pIndexPage->getLabelName(), pIndexPage->getFirstDocument() - m_maxDocsCount);
		}
	}
}

//
// Index forward button click
//
void mainWindow::on_indexForwardButton_clicked(ustring indexName)
{
	IndexPage *pIndexPage = dynamic_cast<IndexPage*>(get_page(indexName, NotebookPageBox::INDEX_PAGE));
	if (pIndexPage != NULL)
	{
		if (pIndexPage->getDocumentsCount() == 0)
		{
#ifdef DEBUG
			cout << "mainWindow::on_indexForwardButton_clicked: first" << endl;
#endif
			browse_index(indexName, pIndexPage->getLabelName(), 0);
		}
		else if (pIndexPage->getDocumentsCount() >= pIndexPage->getFirstDocument() + m_maxDocsCount)
		{
#ifdef DEBUG
			cout << "mainWindow::on_indexForwardButton_clicked: next" << endl;
#endif
			browse_index(indexName, pIndexPage->getLabelName(), pIndexPage->getFirstDocument() + m_maxDocsCount);
		}
#ifdef DEBUG
		cout << "mainWindow::on_indexForwardButton_clicked: counts "
			<< pIndexPage->getFirstDocument() << " " << pIndexPage->getDocumentsCount() << endl;
#endif
	}
}

//
// Query list mouse click
//
bool mainWindow::on_queryTreeview_button_press_event(GdkEventButton *ev)
{
	// Check for double clicks
	if (ev->type == GDK_2BUTTON_PRESS)
	{
		on_editQueryButton_clicked();
	}

	return false;
}

//
// Main window deleted
//
bool mainWindow::on_mainWindow_delete_event(GdkEventAny *ev)
{
	// Any thread still running ?
	if (m_state.get_threads_count() > 0)
	{
		ustring boxTitle = _("At least one task hasn't completed yet. Quit now ?");
		MessageDialog msgDialog(boxTitle, false, MESSAGE_QUESTION, BUTTONS_YES_NO);
		msgDialog.set_transient_for(*this);
		msgDialog.show();
		int result = msgDialog.run();
		if (result == RESPONSE_NO)
		{
			return true;
		}

		m_state.disconnect();
		m_state.stop_threads();
	}
	else
	{
		m_state.disconnect();
	}

	// Disconnect UI signals
	m_pageSwitchConnection.disconnect();

	// Save the window's position and dimensions now
	get_position(m_settings.m_xPos, m_settings.m_yPos);
	get_size(m_settings.m_width, m_settings.m_height);
	m_settings.m_panePos = mainHpaned->get_position();
	m_settings.m_expandQueries = queryExpander->get_expanded();
	m_settings.m_showEngines = enginesTogglebutton->get_active();
#ifdef DEBUG
	cout << "mainWindow::on_mainWindow_delete_event: quitting" << endl;
#endif

	Main::quit();
	return false;
}

//
// Show or hide menuitems.
//
void mainWindow::show_global_menuitems(bool showItems)
{
	// Results menuitems that depend on the page
	clearresults1->set_sensitive(showItems);
}

//
// Show or hide menuitems.
//
void mainWindow::show_selectionbased_menuitems(bool showItems)
{
	// Results menuitems that depend on selection
	viewresults1->set_sensitive(showItems);
	viewcache1->set_sensitive(showItems);
	morelikethis1->set_sensitive(showItems);
	indexresults1->set_sensitive(showItems);

	// Index menuitems that depend on selection
	viewfromindex1->set_sensitive(showItems);
	refreshindex1->set_sensitive(showItems);
	unindex1->set_sensitive(showItems);
	showfromindex1->set_sensitive(showItems);
}

//
// Returns the current page.
//
NotebookPageBox *mainWindow::get_current_page(void)
{
	NotebookPageBox *pNotebookPage = NULL;

	if (m_state.read_lock_lists() == true)
	{
		Widget *pPage = m_pNotebook->get_nth_page(m_pNotebook->get_current_page());
		if (pPage != NULL)
		{
			pNotebookPage = dynamic_cast<NotebookPageBox*>(pPage);
		}

		m_state.unlock_lists();
	}

	return pNotebookPage;
}

//
// Returns the page with the given title.
//
NotebookPageBox *mainWindow::get_page(const ustring &title, NotebookPageBox::PageType type)
{
	NotebookPageBox *pNotebookPage = NULL;

	if (m_state.read_lock_lists() == true)
	{
		for (int pageNum = 0; pageNum < m_pNotebook->get_n_pages(); ++pageNum)
		{
			Widget *pPage = m_pNotebook->get_nth_page(pageNum);
			if (pPage != NULL)
			{
				pNotebookPage = dynamic_cast<NotebookPageBox*>(pPage);
				if (pNotebookPage != NULL)
				{
#ifdef DEBUG
					cout << "mainWindow::get_page: " << pNotebookPage->getTitle() << endl;
#endif
					if ((title == pNotebookPage->getTitle()) &&
						(type == pNotebookPage->getType()))
					{
						// That's the page we are looking for
						break;
					}
					pNotebookPage = NULL;
				}
			}
		}

		m_state.unlock_lists();
	}

	return pNotebookPage;
}

//
// Returns the number of the page with the given title.
//
int mainWindow::get_page_number(const ustring &title, NotebookPageBox::PageType type)
{
	int pageNumber = -1;

	if (m_state.read_lock_lists() == true)
	{
		for (int pageNum = 0; pageNum < m_pNotebook->get_n_pages(); ++pageNum)
		{
			Widget *pPage = m_pNotebook->get_nth_page(pageNum);
			if (pPage != NULL)
			{
				NotebookPageBox *pNotebookPage = dynamic_cast<NotebookPageBox*>(pPage);
				if (pNotebookPage != NULL)
				{
#ifdef DEBUG
					cout << "mainWindow::get_page_number: " << pNotebookPage->getTitle() << endl;
#endif
					if ((title == pNotebookPage->getTitle()) &&
						(type == pNotebookPage->getType()))
					{
						// That's the page we are looking for
						pageNumber = pageNum;
						break;
					}
				}
			}
		}

		m_state.unlock_lists();
	}

	return pageNumber;
}

//
// Edits a query
//
void mainWindow::edit_query(QueryProperties &queryProps, bool newQuery)
{
	string queryName;

	if (newQuery == false)
	{
		// Backup the current name
		queryName = queryProps.getName();
	}

	// Edit the query's properties
	queryDialog queryBox(queryProps);
	queryBox.show();
	if (queryBox.run() != RESPONSE_OK)
	{
		// Nothing to do
		return;
	}

	// Is the name okay ?
	if (queryBox.badName() == true)
	{
		ustring statusText = _("Query name");
		statusText += " ";
		statusText += queryProps.getName();
		statusText += " ";
		statusText +=  _("is already in use");

		// Tell user the name is bad
		set_status(statusText);
		return;
	}

	if (newQuery == false)
	{
		// Did the name change ?
		string newQueryName = queryProps.getName();
		if (newQueryName != queryName)
		{
			QueryHistory history(m_settings.m_historyDatabase);

			// Remove records from QueryHistory
			history.deleteItems(queryName, true);
		}

		// Update the query properties
		if ((m_settings.removeQuery(queryName) == false) ||
			(m_settings.addQuery(queryProps) == false))
		{
			ustring statusText = _("Couldn't update query");
			statusText += " ";
			statusText += queryName;
	
			set_status(statusText);
			return;
		}

		set_status(_("Edited query"));
	}
	else
	{
		// Add the new query
		if (m_settings.addQuery(queryProps) == false)
		{
			ustring statusText = _("Couldn't add query");
			statusText += " ";
			statusText += queryProps.getName();

			set_status(statusText);
			return;
		}

		set_status(_("Added new query"));
	}

	populate_queryTreeview(queryProps.getName());
}

//
// Runs a search
//
void mainWindow::run_search(const QueryProperties &queryProps)
{
	if (queryProps.getFreeQuery().empty() == true)
	{
		set_status(_("Query is not set"));
		return;
	}
#ifdef DEBUG
	cout << "mainWindow::run_search: query name is " << queryProps.getName() << endl;
#endif

	// Check a search engine has been selected
	list<TreeModel::Path> selectedEngines = m_pEnginesTree->getSelection();
	if (selectedEngines.empty() == true)
	{
		set_status(_("No search engine selected"));
		return;
	}

	// Go through the tree and check selected nodes
	list<TreeModel::iterator> engineIters;
	EnginesModelColumns &engineColumns = m_pEnginesTree->getColumnRecord();
	for (list<TreeModel::Path>::iterator enginePath = selectedEngines.begin();
		enginePath != selectedEngines.end(); ++enginePath)
	{
		TreeModel::iterator engineIter = m_pEnginesTree->getIter(*enginePath);
		TreeModel::Row engineRow = *engineIter;

		EnginesModelColumns::EngineType engineType = engineRow[engineColumns.m_type];
		if (engineType < EnginesModelColumns::ENGINE_FOLDER)
		{
			// Skip
			continue;
		}

		// Is it a folder ?
		if (engineType == EnginesModelColumns::ENGINE_FOLDER)
		{
			TreeModel::Children children = engineIter->children();
			for (TreeModel::Children::iterator folderEngineIter = children.begin();
				folderEngineIter != children.end(); ++folderEngineIter)
			{
				TreeModel::Row folderEngineRow = *folderEngineIter;

				EnginesModelColumns::EngineType engineType = engineRow[engineColumns.m_type];
				if (engineType < EnginesModelColumns::ENGINE_FOLDER)
				{
					// Skip
					continue;
				}

				engineIters.push_back(folderEngineIter);
			}
		}
		else if (engineType == EnginesModelColumns::INTERNAL_INDEX_ENGINE)
		{
			// Push this at the beginning of the list
			engineIters.push_front(engineIter);
		}
		else
		{
			engineIters.push_back(engineIter);
		}
	}
#ifdef DEBUG
	cout << "mainWindow::run_search: selected " << engineIters.size()
		<< " engines" << endl;
#endif

	// Now go through the selected search engines
	set<ustring> engineDisplayableNames;
	for (list<TreeModel::iterator>::iterator iter = engineIters.begin();
		iter != engineIters.end(); ++iter)
	{
		TreeModel::Row engineRow = **iter;

		// Check whether this engine has already been done
		// Using a set<TreeModel::iterator/Row> would be preferable
		// but is not helpful here
		ustring engineDisplayableName = engineRow[engineColumns.m_name];
		if (engineDisplayableNames.find(engineDisplayableName) != engineDisplayableNames.end())
		{
			continue;
		}
		engineDisplayableNames.insert(engineDisplayableName);

		ustring engineName = engineRow[engineColumns.m_engineName];
		ustring engineOption = engineRow[engineColumns.m_option];
		EnginesModelColumns::EngineType engineType = engineRow[engineColumns.m_type];
#ifdef DEBUG
		cout << "mainWindow::run_search: engine " << engineDisplayableName << endl;
#endif

		// Is it a web engine ?
		if (engineType == EnginesModelColumns::WEB_ENGINE)
		{
			// There's a special case for the Google API...
			if (engineName == "googleapi")
			{
				// Make sure this has been configured
				if (m_settings.m_googleAPIKey.empty() == true)
				{
					set_status(_("Please set the Google API key first"));
					// Skip this engine
					continue;
				}
				// Option is the Google API key
				engineOption = m_settings.m_googleAPIKey;
			}
		}

		ustring status = _("Running query");
		status += " \"";
		status += to_utf8(queryProps.getName());
		status += "\" ";
		status += _("on");
		status += " ";
		status += engineDisplayableName;
		set_status(status);

		// Spawn a new thread
		start_thread(new QueryingThread(from_utf8(engineName),
			from_utf8(engineDisplayableName), engineOption, queryProps));
	}
}

//
// Browse an index
//
void mainWindow::browse_index(const ustring &indexName, const ustring &labelName,
	unsigned int startDoc, bool changePage)
{
	// Rudimentary lock
	if (m_state.m_browsingIndex == true)
	{
		return;
	}
	m_state.m_browsingIndex = true;

	IndexPage *pIndexPage = dynamic_cast<IndexPage*>(get_page(indexName, NotebookPageBox::INDEX_PAGE));
	if (pIndexPage != NULL)
	{
		IndexTree *pIndexTree = pIndexPage->getTree();
		if (pIndexTree != NULL)
		{
			// Remove existing rows in the index tree
			pIndexTree->clear();
		}
		pIndexPage->setFirstDocument(startDoc);

		if (changePage == true)
		{
			// Switch to that index page
			m_pNotebook->set_current_page(get_page_number(indexName, NotebookPageBox::INDEX_PAGE));
		}
	}

	// Spawn a new thread to browse the index
	IndexBrowserThread *pBrowseThread = new IndexBrowserThread(
		from_utf8(indexName), labelName, m_maxDocsCount, startDoc);
	start_thread(pBrowseThread);
}

//
// View documents
//
void mainWindow::view_documents(vector<DocumentInfo> &documentsList)
{
	ViewHistory viewHistory(m_settings.m_historyDatabase);
	multimap<string, string> locationsByType;
	vector<DocumentInfo>::iterator docIter = documentsList.begin();

	for (vector<DocumentInfo>::iterator docIter = documentsList.begin();
		docIter != documentsList.end(); ++docIter)
	{
		string url(docIter->getLocation());
		string mimeType(docIter->getType());

		if (url.empty() == true)
		{
			continue;
		}

		Url urlObj(url);

		// FIXME: there should be a way to know which protocols can be viewed/indexed
		if (urlObj.getProtocol() == "mailbox")
		{
			DocumentInfo docInfo("", url, "", "");

			// Get that message
			start_thread(new DownloadingThread(docInfo));

			// Record this into the history now, even though it may fail
			if (viewHistory.hasItem(url) == false)
			{
				viewHistory.insertItem(url);
			}
			continue;
		}

		// What's the MIME type ?
		if (mimeType.empty() == true)
		{
			Url urlObj(url);

			// Scan for the MIME type
			mimeType = MIMEScanner::scanUrl(urlObj);
		}

		// Remove the charset, if any
		string::size_type semiColonPos = mimeType.find(";");
		if (semiColonPos != string::npos)
		{
			mimeType.erase(semiColonPos);
		}

#ifdef DEBUG
		cout << "mainWindow::view_documents: " << url << " has type " << mimeType << endl;
#endif
		locationsByType.insert(pair<string, string>(mimeType, url));
	}

	MIMEAction action;
	vector<string> arguments;
	string currentType;

	arguments.reserve(documentsList.size());

	for (multimap<string, string>::iterator locationIter = locationsByType.begin();
		locationIter != locationsByType.end(); ++locationIter)
	{
		string type(locationIter->first);
		string url(locationIter->second);

		if (type != currentType)
		{
			if ((action.m_exec.empty() == false) &&
				(arguments.empty() == false))
			{
				// Run the default program for this type
				if (CommandLine::runAsync(action, arguments) == false)
				{
#ifdef DEBUG
					cout << "mainWindow::view_documents: couldn't view type "
						<< currentType << endl;
#endif
				}

				// Clear the list of arguments
				arguments.clear();
			}

			// Get the action for this MIME type
			if (MIMEScanner::getDefaultAction(type, action) == false)
			{
				Url urlObj(url);
				bool foundAction = false;

				if ((urlObj.getProtocol() == "http") ||
					(urlObj.getProtocol() == "https"))
				{
					// Chances are the web browser will be able to open this
					type = "text/html";
					foundAction = MIMEScanner::getDefaultAction(type, action);
#ifdef DEBUG
					cout << "mainWindow::view_documents: defaulting to text/html" << endl;
#endif
				}
				else if ((type.length() > 5) &&
					(type.substr(0, 5) == "text/"))
				{
					// It's a subtype of text
					// FIXME: MIMEScanner should return parent types !
					type = "text/plain";
					foundAction = MIMEScanner::getDefaultAction(type, action);
#ifdef DEBUG
					cout << "mainWindow::view_documents: defaulting to text/plain" << endl;
#endif
				}

				if (foundAction == false)
				{
					launcherDialog launcher(url);
					bool remember = true;

					launcher.show();
					if (launcher.run() == RESPONSE_OK)
					{

						foundAction = launcher.getInput(action, remember);
					}

					if (foundAction == false)
					{
						ustring statusText = _("No default application defined for type");
						statusText += " ";
						statusText += type;
						set_status(statusText);
						continue;
					}
					else if (remember == true)
					{
						// Add this to MIMESCanner's list
#ifdef DEBUG
						cout << "mainWindow::view_documents: adding user-defined action for type " << type << endl;
#endif
						MIMEScanner::addDefaultAction(type, action);
						// FIXME: save this in the settings ?
					}
				}
			}
		}
		currentType = type;

		arguments.push_back(url);

		// Record this into the history now, even though it may fail
		if (viewHistory.hasItem(url) == false)
		{
			viewHistory.insertItem(url);
		}
	}

	if ((action.m_exec.empty() == false) &&
		(arguments.empty() == false))
	{
		// Run the default program for this type
		if (CommandLine::runAsync(action, arguments) == false)
		{
#ifdef DEBUG
			cout << "mainWindow::view_documents: couldn't view type "
				<< currentType << endl;
#endif
		}
	}
}

//
// Append a document to the index tree.
//
bool mainWindow::append_document(IndexPage *pIndexPage, const ustring &indexName, const DocumentInfo &docInfo)
{
	bool appendToList = true;

	if (pIndexPage == NULL)
	{
		return false;
	}

	IndexTree *pIndexTree = pIndexPage->getTree();
	if (pIndexTree == NULL)
	{
		return false;
	}

	// Does that document have the current label ?
	ustring labelName = pIndexPage->getLabelName();
	if (labelName.empty() == false)
	{
		const std::map<string, string> &indexesMap = m_settings.getIndexes();
		std::map<string, string>::const_iterator mapIter = indexesMap.find(indexName);
		if (mapIter != indexesMap.end())
		{
			IndexInterface *pIndex = m_settings.getIndex(mapIter->second);
	
			if (pIndex != NULL)
			{
				unsigned int indexId = 0;

				appendToList = pIndex->hasLabel(docInfo.getIsIndexed(indexId), from_utf8(labelName));
			}

			delete pIndex;
		}
	}

	if (appendToList == true)
	{
		// Add a row
		pIndexTree->appendDocument(docInfo);

		return true;
	}

	return false;
}

//
// Start of worker thread
//
bool mainWindow::start_thread(WorkerThread *pNewThread, bool inBackground)
{
	if (m_state.start_thread(pNewThread, inBackground) == false)
	{
		// Delete the object
		delete pNewThread;
		return false;
	}
#ifdef DEBUG
	cout << "mainWindow::start_thread: started thread " << pNewThread->getId() << endl;
#endif

	if (inBackground == false)
	{
		// Enable the activity progress bar
		m_timeoutConnection.block();
		m_timeoutConnection.disconnect();
		m_timeoutConnection = Glib::signal_timeout().connect(SigC::slot(*this,
			&mainWindow::on_activity_timeout), 1000);
		m_timeoutConnection.unblock();
	}

	return true;
}

//
// Sets the status bar text.
//
void mainWindow::set_status(const ustring &text, bool canBeSkipped)
{
	static time_t lastTime = time(NULL);

	time_t now = time(NULL);
	if ((difftime(now, lastTime) < 1) &&
		(canBeSkipped == true))
	{
		// Skip this
		return;
	}
	lastTime = now;
	
	unsigned int threadsCount = m_state.get_threads_count();
	if (threadsCount > 0)
	{
		char threadsCountStr[64];

		snprintf(threadsCountStr, 64, "%u", threadsCount);
		// Display the number of threads
		mainProgressbar->set_text(threadsCountStr);
	}
	else
	{
		// Reset
		mainProgressbar->set_text("");
	}
	// Pop the previous message
	mainStatusbar->pop();
	// Push
	mainStatusbar->push(text);
}
