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

#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <string>
#include <iostream>
#include <fstream>
#include <sigc++/class_slot.h>
#include <glibmm/ustring.h>
#include <glibmm/stringutils.h>
#include <glibmm/convert.h>
#include <glibmm/thread.h>
#include <gtkmm/stock.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/clipboard.h>
#include <gtkmm/main.h>

#include "HtmlTokenizer.h"
#include "IndexedDocument.h"
#include "TimeConverter.h"
#include "Url.h"
#include "ActionHistory.h"
#include "QueryHistory.h"
#include "ViewHistory.h"
#include "DownloaderFactory.h"
#include "XapianIndex.h"
#include "SearchEngineFactory.h"
#include "XapianEngine.h"
#include "config.h"
#include "NLS.h"
#include "IndexPage.h"
#include "PinotUtils.h"
#include "mainWindow.hh"
#include "aboutDialog.hh"
#include "importDialog.hh"
#include "indexDialog.hh"
#include "propertiesDialog.hh"
#include "prefsDialog.hh"
#include "queryDialog.hh"

using namespace std;
using namespace Glib;
using namespace Gdk;
using namespace Gtk;

// A function object to delete pointers from a set with for_each()
struct DeleteSetPointer
{
	template<typename T>
	void operator()(const T *ptr) const
	{
		delete ptr;
	}
};

// FIXME: this ought to be configurable
unsigned int mainWindow::m_maxDocsCount = 100;
unsigned int mainWindow::m_maxThreads = 2;

mainWindow::InternalState::InternalState() :
	m_currentPage(0),
	m_backgroundThreads(0),
	m_browsingIndex(false)
{
	pthread_rwlock_init(&m_rwLock, NULL);
}

mainWindow::InternalState::~InternalState()
{
	// Destroy the read/write lock
	pthread_rwlock_destroy(&m_rwLock);
}

bool mainWindow::InternalState::readLock(unsigned int where)
{
	if (pthread_rwlock_rdlock(&m_rwLock) == 0)
	{
#ifdef DEBUG
		cout << "mainWindow::InternalState::readLock " << where << endl;
#endif
		return true;
	}

	return false;
}

bool mainWindow::InternalState::writeLock(unsigned int where)
{
	if (pthread_rwlock_wrlock(&m_rwLock) == 0)
	{
#ifdef DEBUG
		cout << "mainWindow::InternalState::writeLock " << where << endl;
#endif
		return true;
	}

	return false;
}

void mainWindow::InternalState::unlock(void)
{
#ifdef DEBUG
	cout << "mainWindow::InternalState::unlock" << endl;
#endif
	pthread_rwlock_unlock(&m_rwLock);
}

//
// Constructor
//
mainWindow::mainWindow() :
	m_settings(PinotSettings::getInstance()), mainWindow_glade(),
	m_pEnginesTree(NULL),
	m_pNotebook(NULL),
	m_pIndexMenu(NULL),
	m_pLabelsMenu(NULL),
	m_pHtmlView(NULL)
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

	// Position the engine tree
	m_pEnginesTree = manage(new EnginesTree(enginesVbox, m_settings));
	// Connect to the "changed" signal
	m_pEnginesTree->get_selection()->signal_changed().connect(SigC::slot(*this,
		&mainWindow::on_enginesTreeviewSelection_changed));
	// Connect to the edit index signal
	m_pEnginesTree->getEditIndexSignal().connect(SigC::slot(*this,
		&mainWindow::on_editindex));

	// Set an icon for this and other windows
	set_icon_from_file("/usr/share/icons/hicolor/48x48/apps/pinot.png");
	set_default_icon_from_file("/usr/share/icons/hicolor/48x48/apps/pinot.png");

	// Associate the columns model to the query tree
	m_refQueryTree = ListStore::create(m_queryColumns);
	queryTreeview->set_model(m_refQueryTree);
	TreeViewColumn *treeColumn = create_resizable_column(_("Query Name"), m_queryColumns.m_name);
	if (treeColumn != NULL)
	{
		queryTreeview->append_column(*manage(treeColumn));
	}
	queryTreeview->append_column(_("Last Run"), m_queryColumns.m_lastRun);
	queryTreeview->append_column(_("Summary"), m_queryColumns.m_summary);
	// Allow only single selection
	queryTreeview->get_selection()->set_mode(SELECTION_SINGLE);
	// Connect to the "changed" signal
	queryTreeview->get_selection()->signal_changed().connect(SigC::slot(*this, 
		&mainWindow::on_queryTreeviewSelection_changed));
	// Populate
	populate_queryTreeview();

	// Populate the index menu
	populate_indexMenu();

	// Create a notebook, without any page initially
	m_pNotebook = manage(new Notebook());
	m_pNotebook->set_flags(Gtk::CAN_FOCUS);
	m_pNotebook->set_show_tabs(true);
	m_pNotebook->set_show_border(true);
	m_pNotebook->set_tab_pos(Gtk::POS_TOP);
	m_pNotebook->set_scrollable(false);
	rightVbox->pack_start(*m_pNotebook, Gtk::PACK_EXPAND_WIDGET, 4);
	m_pNotebook->signal_switch_page().connect(
		SigC::slot(*this, &mainWindow::on_switch_page), false);

	// Create an HTML renderer
	m_pHtmlView = new HtmlView(NULL);
	if (m_settings.m_browseResults == false)
	{
		view_document("file:///usr/share/pinot/index.html", true);
	}

	// Gray out menu items
	editQueryButton->set_sensitive(false);
	removeQueryButton->set_sensitive(false);
	findQueryButton->set_sensitive(false);
	clearresults1->set_sensitive(false);
	showextract1->set_sensitive(false);
	groupresults1->set_sensitive(false);
	viewresults1->set_sensitive(false);
	// Hide the View Cache menu item ?
	if (SearchEngineFactory::isSupported("googleapi") == false)
	{
		viewcache1->hide();
	}
	else
	{
		viewcache1->set_sensitive(false);
	}
	indexresults1->set_sensitive(false);
	viewfromindex1->set_sensitive(false);
	refreshindex1->set_sensitive(false);
	unindex1->set_sensitive(false);
	showfromindex1->set_sensitive(false);
	//viewstop1->set_sensitive(false);
	// ...and buttons
	removeIndexButton->set_sensitive(false);

	// Set focus on the query entry field
	set_focus(*liveQueryEntry);

	// Set tooltips
	m_tooltips.set_tip(*addIndexButton, _("Add index"));
	m_tooltips.set_tip(*removeIndexButton, _("Remove index"));

	// FIXME: delete all "ignored" threads when exiting !!!
	// Fire up a listener thread
	ListenerThread *pListenThread = new ListenerThread(PinotSettings::getConfigurationDirectory() + string("/fifo"));
	// Connect to its reception signal
	pListenThread->getReceptionSignal().connect(SigC::slot(*this,
		&mainWindow::on_message_reception));
	start_thread(pListenThread, true);

	// Fire up the mail monitor thread
	MboxHandler *pMbox = new MboxHandler();
	// Connect to its update signal
	pMbox->getUpdateSignal().connect(SigC::slot(*this,
		&mainWindow::on_message_indexupdate));
	MonitorThread *pMonitorThread = new MonitorThread(pMbox);
	start_thread(pMonitorThread, true);
	// The handler object will be deleted when the thread terminates

	// There might be queued actions
	check_queue();

	// Now we are ready
	set_status(_("Ready"));
	m_pNotebook->show();
	show();
}

//
// Destructor
//
mainWindow::~mainWindow()
{
	// Save queries
	save_queryTreeview();

	if (m_state.m_pThreads.empty() == false)
	{
		for_each(m_state.m_pThreads.begin(), m_state.m_pThreads.end(), DeleteSetPointer());
	}

	// Stop in case we were loading a page
	NotebookPageBox *pNotebookPage = get_page(_("View"), NotebookPageBox::VIEW_PAGE);
	if (pNotebookPage != NULL)
	{
		m_pHtmlView->stop();
	}
	delete m_pHtmlView;
}

//
// Load user-defined queries
//
void mainWindow::populate_queryTreeview()
{
	QueryHistory history(m_settings.m_historyDatabase);
	const std::map<string, QueryProperties> &queries = m_settings.getQueries();

	// Reset the whole tree
	queryTreeview->get_selection()->unselect_all();
	m_refQueryTree->clear();

	// Add all user-defined queries
	std::map<string, QueryProperties>::const_iterator queryIter = queries.begin();
	for (; queryIter != queries.end(); ++queryIter)
	{
		TreeModel::iterator iter = m_refQueryTree->append();
		TreeModel::Row row = *iter;
		string queryName = queryIter->first;

		row[m_queryColumns.m_name] = to_utf8(queryName);
		string lastRun = history.getLastRun(queryName);
		if (lastRun.empty() == true)
		{
			lastRun = _("N/A");
		}
		row[m_queryColumns.m_lastRun] = lastRun;
		string summary = queryIter->second.toString();
		if (summary.empty() == false)
		{
			row[m_queryColumns.m_summary] = to_utf8(summary);
		}
		else
		{
			row[m_queryColumns.m_summary] = _("<undefined>");
		}
		row[m_queryColumns.m_properties] = queryIter->second;
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
			string name = locale_from_utf8(row[m_queryColumns.m_name]);
			QueryProperties queryProps = row[m_queryColumns.m_properties];
#ifdef DEBUG
			cout << "mainWindow::save_queryTreeview: " << name << endl;
#endif
			m_settings.addQuery(queryProps);
		}
	}
}

//
// Populate the index menu
//
void mainWindow::populate_indexMenu()
{
	if (m_pIndexMenu == NULL)
	{
		m_pIndexMenu = new Menu();
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
	SigC::Slot0<void> documentsActivateSlot = sigc::bind(indexSlot, _("My Documents"));
	pMenuItem->signal_activate().connect(documentsActivateSlot);

	m_pIndexMenu->items().push_back(Menu_Helpers::MenuElem(_("My Email")));
	pMenuItem = &m_pIndexMenu->items().back();
	// Bind the callback's parameter to the index name
	SigC::Slot0<void> emailActivateSlot = sigc::bind(indexSlot, _("My Email"));
	pMenuItem->signal_activate().connect(emailActivateSlot);
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
	TreeModel::Row engineRow = *engineIter;
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
	ustring url;
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
				hasSelection = pResultsTree->checkSelection();
				if (hasSelection == true)
				{
					url = pResultsTree->getFirstSelectionURL();
				}
			}
		}
	}

	if (hasSelection == true)
	{
		bool isViewable = true, isIndexable = true, isCached = false;

		Url urlObj(locale_from_utf8(url));
		string protocol = urlObj.getProtocol();
		// FIXME: there should be a way to know which protocols can be viewed/indexed
		if (protocol == "xapian")
		{
			isViewable = isIndexable = false;
		}

		// Enable these menu items
		viewresults1->set_sensitive(isViewable);
		if (m_settings.m_browseResults == false)
		{
			if ((protocol == "http") ||
				(protocol == "https"))
			{
				isCached = true;
			}
		}
		viewcache1->set_sensitive(isCached);
		indexresults1->set_sensitive(isIndexable);

		// Show the URL in the status bar
		ustring statusText = _("Result location is");
		statusText += " ";
		statusText += url;
		set_status(statusText, true);
	}
	else
	{
		// Disable these menu items
		viewresults1->set_sensitive(false);
		viewcache1->set_sensitive(false);
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
	vector<IndexedDocument> documentsList;

	IndexPage *pIndexPage = dynamic_cast<IndexPage*>(get_page(indexName, NotebookPageBox::INDEX_PAGE));
	if (pIndexPage == NULL)
	{
		return;
	}

	IndexTree *pIndexTree = pIndexPage->getTree();
	if ((pIndexTree != NULL) &&
		(pIndexTree->getSelection(documentsList) == true))
	{
		bool isDocumentsIndex = true;
		bool canViewDocument = true;

		// Enable these menu items, unless the index is not the documents index
		if (indexName != _("My Documents"))
		{
			isDocumentsIndex = false;
		}
		if ((indexName == _("My Email")) &&
			(m_settings.m_browseResults == true))
		{
			canViewDocument = false;
		}
		viewfromindex1->set_sensitive(canViewDocument);
		refreshindex1->set_sensitive(isDocumentsIndex);
		unindex1->set_sensitive(isDocumentsIndex);
		showfromindex1->set_sensitive(true);

		// Show the URL in the status bar
		ustring statusText = _("Document location is");
		statusText += " ";
		statusText += pIndexTree->getFirstSelectionURL();
		set_status(statusText, true);
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
// Index > List menu selected
//
void mainWindow::on_index_changed(ustring indexName)
{
	IndexTree *pIndexTree = NULL;
	IndexPage *pIndexPage = NULL;
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
		pIndexPage->setFirstDocument(0);
		pIndexPage->setDocumentsCount(0);
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
		if (m_state.writeLock(1) == true)
		{
			int pageNum = m_pNotebook->append_page(*pIndexPage, *pTab);
			m_pNotebook->pages().back().set_tab_label_packing(false, true, Gtk::PACK_START);
			if (pageNum >= 0)
			{
				foundPage = true;
			}

			m_state.unlock();
		}
	}

	if (foundPage == true)
	{
		browse_index(indexName, 0);
	}
}

//
// Index > labels combo selection changed
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
	pIndexTree->setCurrentLabelColour(0, 0, 0, false);
	if (labelName.empty() == true)
	{
		// User selected no label
		set_status(_("No label"));
		return;
	}

	// Get the documents that match this label
	start_thread(new LabelQueryThread(indexName, labelName));
}

//
// Notebook page switch
//
void mainWindow::on_switch_page(GtkNotebookPage *p0, guint p1)
{
	// Did the page change ?
	if (m_state.m_currentPage != p1)
	{
		bool showResultsMenuitems = false;

		NotebookPageBox *pNotebookPage = dynamic_cast<NotebookPageBox*>(m_pNotebook->get_nth_page(p1));
		if (pNotebookPage != NULL)
		{
			NotebookPageBox::PageType type = pNotebookPage->getType();
			if (type == NotebookPageBox::RESULTS_PAGE)
			{
				showResultsMenuitems = true;
			}
#ifdef DEBUG
			cout << "mainWindow::on_switch_page: switched to page " << p1
				<< ", type " << type << endl;
#endif
		}

		// Results menuitems that depend on the page
		clearresults1->set_sensitive(showResultsMenuitems);
		showextract1->set_sensitive(showResultsMenuitems);
		groupresults1->set_sensitive(showResultsMenuitems);

		// Results menuitems that depend on selection
		viewresults1->set_sensitive(false);
		viewcache1->set_sensitive(false);
		indexresults1->set_sensitive(false);

		// Index menuitems that depend on selection
		viewfromindex1->set_sensitive(false);
		refreshindex1->set_sensitive(false);
		unindex1->set_sensitive(false);
		showfromindex1->set_sensitive(false);
	}
	m_state.m_currentPage = (int)p1;
}

//
// Notebook page closed
//
void mainWindow::on_close_page(ustring title, NotebookPageBox::PageType type)
{
#ifdef DEBUG
	cout << "mainWindow::on_close_page: called for tab " << title << endl;
#endif
	int pageNum = get_page_number(title, type);
	if (pageNum >= 0)
	{
		if (type == NotebookPageBox::VIEW_PAGE)
		{
			// Stop rendering
			m_pHtmlView->stop();

			// Hide, don't close the page
			Widget *pPage = m_pNotebook->get_nth_page(pageNum);
			if (pPage != NULL)
			{
				pPage->hide();
			}
		}
		else if (m_state.writeLock(2) == true)
		{
			// Remove the page
			m_pNotebook->remove_page(pageNum);

			m_state.unlock();
		}
	}
}

//
// End of worker thread
//
void mainWindow::on_thread_end()
{
	WorkerThread *pThread = NULL;
	ustring status;

	// Get the first thread that's finished
	if (m_state.readLock(3) == true)
	{
		for (set<WorkerThread *>::iterator threadIter = m_state.m_pThreads.begin();
			threadIter != m_state.m_pThreads.end(); ++threadIter)
		{
#ifdef DEBUG
			cout << "mainWindow::on_thread_end: looking for thread" << endl;
#endif
			if ((*threadIter)->isDone() == true)
			{
				// This one will do...
				pThread = (*threadIter);
				// Remove it
				m_state.m_pThreads.erase(threadIter);
				break;
			}
		}

		m_state.unlock();
	}
	if (pThread == NULL)
	{
#ifdef DEBUG
		cout << "mainWindow::on_thread_end: signalled but couldn't find any thread" << endl;
#endif
		return;
	}
#ifdef DEBUG
	cout << "mainWindow::on_thread_end: end of thread " << pThread->getId() << endl;
#endif
	update_threads_status();

	// What type of thread was it ?
	string type = pThread->getType();
	if (pThread->isBackground() == true)
	{
		m_state.m_backgroundThreads--;
	}
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
		unsigned int count = 0;

		IndexBrowserThread *pBrowseThread = dynamic_cast<IndexBrowserThread *>(pThread);
		if (pBrowseThread == NULL)
		{
			delete pThread;
			return;
		}

		IndexPage *pIndexPage = NULL;
		IndexTree *pIndexTree = NULL;
		ustring indexName = locale_to_utf8(pBrowseThread->getIndexName());

		// Find the page for this index
		pIndexPage = dynamic_cast<IndexPage*>(get_page(indexName, NotebookPageBox::INDEX_PAGE));
		if (pIndexPage == NULL)
		{
			// It's probably been closed by the user
			return;
		}
		pIndexTree = pIndexPage->getTree();
		if (pIndexTree == NULL)
		{
			return;
		}

		pIndexPage->setDocumentsCount(pBrowseThread->getDocumentsCount());
		count = pIndexTree->getRowsCount();

		status = _("Showing");
		status += " ";
		snprintf(docsCountStr, 64, "%u", count);
		status += docsCountStr;
		status += " ";
		status += _("off");
		status += " ";
		snprintf(docsCountStr, 64, "%u", pIndexPage->getDocumentsCount());
		status += docsCountStr;
		status += " ";
		status += _("documents from");
		status += " ";
		status += indexName;
		set_status(status);

		if (pIndexPage->getDocumentsCount() > 0)
		{
			// Refresh the tree
			pIndexTree->refresh();
		}
		pIndexPage->updateButtons(m_maxDocsCount);
		m_state.m_browsingIndex = false;
	}
	else if (type == "QueryingThread")
	{
		std::map<string, TreeModel::iterator> updatedGroups;
		unsigned int count = 0;
		ResultsModelColumns::ResultType rootType;
		bool mergeDuplicates = false;
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
		const vector<Result> &resultsList = pQueryThread->getResults();

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
			if (pResultsTree == NULL)
			{
				return;
			}
			pageNum = get_page_number(queryName, NotebookPageBox::RESULTS_PAGE);
		}
		else
		{
			// There is none, create one
			NotebookTabBox *pTab = manage(new NotebookTabBox(queryName,
				NotebookPageBox::RESULTS_PAGE));
			pTab->getCloseSignal().connect(
				SigC::slot(*this, &mainWindow::on_close_page));

			// Position the results tree
			pResultsTree = manage(new ResultsTree(queryName, resultsMenuitem->get_submenu(), m_settings));
			pResultsPage = manage(new ResultsPage(queryName, pResultsTree, m_settings));
			// Connect to the "changed" signal
			pResultsTree->getSelectionChangedSignal().connect(
				SigC::slot(*this, &mainWindow::on_resultsTreeviewSelection_changed));

			// Append the page
			if (m_state.writeLock(1) == true)
			{
				pageNum = m_pNotebook->append_page(*pResultsPage, *pTab);
				m_pNotebook->pages().back().set_tab_label_packing(false, true, Gtk::PACK_START);

				m_state.unlock();
			}
		}

		if (pageNum >= 0)
		{
			// Add the results to the tree
			pResultsTree->addResults(queryProps, engineName,
				resultsList, searchenginegroup1->get_active());
			// Switch to that page
			m_pNotebook->set_current_page(pageNum);
		}

		// Index results ?
		if ((queryProps.getIndexResults() == true) &&
			(resultsList.empty() == false))
		{
			string labelName = queryProps.getLabelName();

#ifdef DEBUG
			cout << "mainWindow::on_thread_end: indexing results, with label " << labelName << endl;
#endif
			for (vector<Result>::const_iterator resultIter = resultsList.begin();
				resultIter != resultsList.end(); ++resultIter)
			{
				// Queue this action
				queue_index(DocumentInfo(resultIter->getTitle(), resultIter->getLocation(),
					resultIter->getType(), resultIter->getLanguage()),
					labelName);
			}
		}
	}
	else if (type == "LabelQueryThread")
	{
		LabelQueryThread *pLabelQueryThread = dynamic_cast<LabelQueryThread *>(pThread);
		if (pLabelQueryThread == NULL)
		{
			delete pThread;
			return;
		}
		ustring indexName = locale_to_utf8(pLabelQueryThread->getIndexName());

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

		// Don't bother if the index is not being listed, or if the user
		// changed the label in the meantime
		ustring labelName = pIndexPage->getLabelName();
		if ((pIndexTree->isEmpty() == false) &&
			(labelName.empty() == false) &&
			(labelName == locale_to_utf8(pLabelQueryThread->getLabelName())))
		{
			const set<unsigned int> &documentsList = pLabelQueryThread->getDocumentsList();
			char docsCountStr[64];
			unsigned int docsCount = documentsList.size();

			status = _("Label");
			status += " ";
			status += labelName;
			status += " ";
			status += _("matches");
			status += " ";
			snprintf(docsCountStr, 64, "%u", docsCount);
			status += docsCountStr;
			status += " ";
			status += _("document(s)");
			set_status(status);

#ifdef DEBUG
			cout << "mainWindow::on_thread_end: current label is " << labelName << endl;
#endif
			// Get the actual label colour from the settings
			for (set<PinotSettings::Label>::iterator labelIter = m_settings.m_labels.begin();
				labelIter != m_settings.m_labels.end(); ++labelIter)
			{
				if (labelName == labelIter->m_name)
				{
					// Display the selected label's colour in the index tree
					pIndexTree->setCurrentLabelColour(labelIter->m_red, labelIter->m_green, labelIter->m_blue);
					pIndexTree->setLabel(documentsList);

					// Switch to the index page
					m_pNotebook->set_current_page(get_page_number(indexName, NotebookPageBox::INDEX_PAGE));
					break;
				}
			}
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
				// Make sure settings haven't changed in the meantime
				if (m_settings.m_browseResults == false)
				{
					ustring viewName = _("View");

					// Is there still a view page ?
					int pageNum = get_page_number(viewName, NotebookPageBox::VIEW_PAGE);
					if (pageNum >= 0)
					{
						// Display the URL in the View tab
						if (m_pHtmlView->renderData(pData, dataLength, url) == true)
						{
							//viewstop1->set_sensitive(true);
							set_status(locale_to_utf8(url));
						}

						m_pNotebook->set_current_page(pageNum);
					}
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

		const set<unsigned int> &docIdList = pIndexThread->getDocumentIDs();

		// Did the thread perform an update ?
		if (pIndexThread->isNewDocument() == false)
		{
			// Yes, it did
			status = _("Updated document");
		}
		else
		{
			string url = pIndexThread->getURL();
			bool labeled = false;

			status = _("Indexed");
			status += " ";
			status += to_utf8(url);

			// Update the in-progress list
			if (m_state.writeLock(4) == true)
			{
				set<string>::iterator urlIter = m_state.m_beingIndexed.find(url);
				if (urlIter != m_state.m_beingIndexed.end())
				{
					m_state.m_beingIndexed.erase(urlIter);
				}

				m_state.unlock();
			}

			// Is the index still being shown ?
			IndexPage *pIndexPage = dynamic_cast<IndexPage*>(get_page(_("My Documents"), NotebookPageBox::INDEX_PAGE));
			if (pIndexPage != NULL)
			{
				IndexTree *pIndexTree = pIndexPage->getTree();
				XapianIndex index(m_settings.m_indexLocation);

				// Was the current label applied to that document ?
				ustring labelName = pIndexPage->getLabelName();
				if ((labelName.empty() == false) &&
					(labelName == locale_to_utf8(pIndexThread->getLabelName())))
				{
					labeled = true;
				}

				if ((pIndexTree != NULL) &&
					(index.isGood() == true))
				{
					// Update the index tree
					for (set<unsigned int>::iterator idIter = docIdList.begin();
						idIter != docIdList.end(); ++idIter)
					{
						DocumentInfo docInfo;
						unsigned int docId = *idIter;

						// Get that document's properties
						if (index.getDocumentInfo(docId, docInfo) == true)
						{
							// Append to the index tree
							IndexedDocument indexedDoc(docInfo.getTitle(),
								XapianEngine::buildUrl(m_settings.m_indexLocation, docId),
								docInfo.getLocation(), docInfo.getType(),
								docInfo.getLanguage());
							indexedDoc.setTimestamp(docInfo.getTimestamp());
							pIndexTree->appendDocument(indexedDoc, labeled);
						}
					}
				}
			}
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
			status = _("Unindexed document(s)");
			set_status(status);
		}
		// Else, stay silent
	}
	else if (type == "MonitorThread")
	{
		// FIXME: do something about this
	}
	else if (type == "UpdateDocumentThread")
	{
		UpdateDocumentThread *pUpdateThread = dynamic_cast<UpdateDocumentThread *>(pThread);
		if (pUpdateThread == NULL)
		{
			delete pThread;
			return;
		}

		IndexPage *pIndexPage = dynamic_cast<IndexPage*>(get_page(_("My Documents"), NotebookPageBox::INDEX_PAGE));
		if (pIndexPage != NULL)
		{
			IndexTree *pIndexTree = pIndexPage->getTree();
			if (pIndexTree != NULL)
			{
				pIndexTree->setDocumentTitle(pUpdateThread->getDocumentID(),
					pUpdateThread->getDocumentInfo().getTitle());
			}
		}

		status = _("Updated document");
		set_status(status);
	}
	else if (type == "ListenerThread")
	{
		// FIXME: do something about this
	}

	// Delete the thread
	delete pThread;;

	// We might be able to run a queued action
	check_queue();

	// Any threads left to return ?
	if (get_threads_count() == 0)
	{
#ifdef DEBUG
		cout << "mainWindow::on_thread_end: disconnecting timeout" << endl;
#endif
		if (m_timeoutConnection.connected() == true)
		{
			m_timeoutConnection.block();
			m_timeoutConnection.disconnect();
			mainProgressbar->set_fraction(0.0);
		}
#ifdef DEBUG
		else cout << "mainWindow::on_thread_end: not connected" << endl;
#endif
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
		if ((m_settings.removeIndex(locale_from_utf8(indexName)) == false) ||
			(m_settings.addIndex(locale_from_utf8(newName),
				locale_from_utf8(newLocation)) == false))
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
// Message reception from ListenerThread
//
void mainWindow::on_message_reception(DocumentInfo docInfo, string labelName)
{
	string location = docInfo.getLocation();

	if (location.empty() == false)
	{
#ifdef DEBUG
		cout << "mainWindow::on_message_reception: indexing " << location << endl;
#endif
		// Queue this
		queue_index(docInfo, labelName);
	}
}

//
// Message reception from IndexBrowserThread
//
void mainWindow::on_message_indexupdate(IndexedDocument docInfo, unsigned int docId, string indexName)
{
	IndexPage *pIndexPage = NULL;
	IndexTree *pIndexTree = NULL;
	bool hasLabel = false;

	// Find the page for this index
	pIndexPage = dynamic_cast<IndexPage*>(get_page(locale_to_utf8(indexName), NotebookPageBox::INDEX_PAGE));
	if (pIndexPage == NULL)
	{
		// It's probably been closed by the user
#ifdef DEBUG
		cout << "mainWindow::on_message_indexupdate: " << indexName << " not being shown" << endl;
#endif
		return;
	}
	pIndexTree = pIndexPage->getTree();
	if (pIndexTree == NULL)
	{
		return;
	}

	// Is the last page being displayed ?
	if (pIndexPage->getFirstDocument() + m_maxDocsCount < pIndexPage->getDocumentsCount())
	{
		// No, so we can't add a new entry for that document
		// Increment the count
		pIndexPage->setDocumentsCount(pIndexPage->getDocumentsCount() + 1);
		// ...and make sure the user can display that last page
		pIndexPage->updateButtons(m_maxDocsCount);
		return;
	}

	// Does that document have the current label ?
	ustring labelName = pIndexPage->getLabelName();
	if (labelName.empty() == false)
	{
		const std::map<string, string> &indexesMap = PinotSettings::getInstance().getIndexes();
		std::map<string, string>::const_iterator mapIter = indexesMap.find(indexName);
		if (mapIter != indexesMap.end())
		{
			XapianIndex index(mapIter->second);

			if (index.isGood() == true)
			{
				hasLabel = index.hasLabel(docId, locale_from_utf8(labelName));
			}
		}
	}

	// Add a row
	if (pIndexTree->appendDocument(docInfo, hasLabel) == true)
	{
#ifdef DEBUG
		cout << "mainWindow::on_message_indexupdate: added document to index list" << endl;
#endif
		pIndexPage->setDocumentsCount(pIndexPage->getDocumentsCount() + 1);
	}
}

//
// Message reception from importDialog
//
void mainWindow::on_message_import(DocumentInfo docInfo)
{
	string location = docInfo.getLocation();

	if (location.empty() == false)
	{
		// Index the selected file
		queue_index(docInfo, "");
	}
}

//
// Session > Configure menu selected
//
void mainWindow::on_configure_activate()
{
	bool useExternalBrowser = m_settings.m_browseResults;

	prefsDialog prefsBox;
	prefsBox.show();
	if (prefsBox.run() != RESPONSE_OK)
	{
		return;
	}
#ifdef DEBUG
	cout << "mainWindow::on_configure_activate: settings changed" << endl;
#endif

	// FIXME: if mail accounts are configured, make sure the MonitorThread
	// is running and knows about the new accounts

	if (m_state.readLock(5) == true)
	{
		for (int pageNum = 0; pageNum < m_pNotebook->get_n_pages(); ++pageNum)
		{
			Widget *pPage = m_pNotebook->get_nth_page(pageNum);
			if (pPage != NULL)
			{
				IndexPage *pIndexPage = dynamic_cast<IndexPage*>(pPage);
				if (pIndexPage != NULL)
				{
					IndexTree *pIndexTree = pIndexPage->getTree();

					// Synchronize the labels list with the new settings
					if (pIndexTree != NULL)
					{
						pIndexTree->setCurrentLabelColour(0, 0, 0, false);
					}
					pIndexPage->populateLabelCombobox();
				}
			}
		}

		m_state.unlock();
	}

	// Do the changes affect the View tab ?
	if (useExternalBrowser != m_settings.m_browseResults)
	{
		if (m_settings.m_browseResults == true)
		{
			// Close the existing view page
			on_close_page(_("View"), NotebookPageBox::VIEW_PAGE);
		}
		else
		{
			// Reopen the view page
			view_document("file:///usr/share/pinot/index.html", true);
		}
	}

	// Any labels to delete or rename ?
	const set<string> &labelsToDelete = prefsBox.getLabelsToDelete();
	const std::map<string, string> &labelsToRename = prefsBox.getLabelsToRename();
	if ((labelsToDelete.empty() == false) ||
		(labelsToRename.empty() == false))
	{
		start_thread(new LabelUpdateThread(labelsToDelete, labelsToRename));
	}

	// Any mail documents we should delete ?
	const set<string> &mailLabelsToDelete = prefsBox.getMailLabelsToDelete();
	if (mailLabelsToDelete.empty() == false)
	{
		start_thread(new UnindexingThread(mailLabelsToDelete, locale_from_utf8(m_settings.m_mailIndexLocation)));
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
			ResultsPage *pResultsPage = dynamic_cast<ResultsPage*>(pNotebookPage);
			if (pResultsPage != NULL)
			{
				vector<Result> resultsList;
				bool firstItem = true;

#ifdef DEBUG
				cout << "mainWindow::on_copy_activate: results tree" << endl;
#endif
				ResultsTree *pResultsTree = pResultsPage->getTree();
				if (pResultsTree != NULL)
				{
					// Get the current results selection
					pResultsTree->getSelection(resultsList);
				}

				for (vector<Result>::const_iterator resultIter = resultsList.begin();
					resultIter != resultsList.end(); ++resultIter)
				{
					if (firstItem == false)
					{
						text += "\n";
					}
					text += resultIter->getTitle();
					text += " ";
					text += resultIter->getLocation();
					firstItem = false;
				}
			}

			IndexPage *pIndexPage = dynamic_cast<IndexPage*>(pNotebookPage);
			if (pIndexPage != NULL)
			{
				vector<IndexedDocument> documentsList;
				bool firstItem = true;

#ifdef DEBUG
				cout << "mainWindow::on_copy_activate: index tree" << endl;
#endif
				IndexTree *pIndexTree = pIndexPage->getTree();
				if (pIndexTree != NULL)
				{
					// Get the current documents selection
					pIndexTree->getSelection(documentsList);
				}

				for (vector<IndexedDocument>::const_iterator docIter = documentsList.begin();
					docIter != documentsList.end(); ++docIter)
				{
					if (firstItem == false)
					{
						text += "\n";
					}
					text += docIter->getTitle();
					text += " ";
					text += docIter->getLocation();
					firstItem = false;
				}
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
	refClipboard->set_text(text);
}

//
// Edit > Paste menu selected
//
void mainWindow::on_paste_activate()
{
	RefPtr<Clipboard> refClipboard = Clipboard::get();
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
		QueryProperties queryProps = QueryProperties(locale_from_utf8(clipText),
			"", "", "", "");
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
	ResultsModelColumns::ResultType currentType, newType;

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
				pResultsTree->regroupResults(searchenginegroup1->get_active());
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
				ustring url = pResultsTree->getFirstSelectionURL();
				if (view_document(locale_from_utf8(url)) == true)
				{
					// We can update the row right now
					pResultsTree->setFirstSelectionViewedState(true);
				}
			}
		}
	}
}

//
// Results > View Cache menu selected
//
void mainWindow::on_viewcache_activate()
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
				ustring url = pResultsTree->getFirstSelectionURL();

				start_thread(new DownloadingThread(url, true));

				// Update the row now, even though the cached page may not be retrieved
				pResultsTree->setFirstSelectionViewedState(true);
			}
		}
	}
}

//
// Results > Index menu selected
//
void mainWindow::on_indexresults_activate()
{
	vector<Result> resultsList;

	// Make sure this has been configured
	if (m_settings.m_indexLocation.empty() == true)
	{
		set_status(_("Please set a location for the index first"));
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
				pResultsTree->getSelection(resultsList);
			}
		}
	}

	// Go through selected results
	for (vector<Result>::const_iterator resultIter = resultsList.begin();
		resultIter != resultsList.end(); ++resultIter)
	{
		// Get the actual URL to download
		string url = resultIter->getLocation();
	
		if (url.empty() == true)
		{
			set_status(_("Result location is unknown"));
			return;
		}
#ifdef DEBUG
		cout << "mainWindow::on_indexresults_activate: URL is " << url << endl;
#endif
		queue_index(DocumentInfo(resultIter->getTitle(), url,
			resultIter->getType(), resultIter->getLanguage()), "");
	}
}

//
// Index > Import menu selected
//
void mainWindow::on_import_activate()
{
	importDialog importBox(_("Import Document(s)"));

	importBox.getImportFileSignal().connect(SigC::slot(*this,
		&mainWindow::on_message_import));
	importBox.show();
	importBox.run();
	// Let the signal handler deal with importing stuff
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
			// View the first document, don't bother about the rest
			ustring url = pIndexTree->getFirstSelectionLiveURL();
			view_document(locale_from_utf8(url));
		}
	}
}

//
// Index > Refresh menu selected
//
void mainWindow::on_refreshindex_activate()
{
	vector<IndexedDocument> documentsList;

	// Make sure this has been configured
	if (m_settings.m_indexLocation.empty() == true)
	{
		set_status(_("Please set a location for the index first"));
		return;
	}

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

	for (vector<IndexedDocument>::const_iterator docIter = documentsList.begin();
		docIter != documentsList.end(); ++docIter)
	{
		// The document ID
		unsigned int docId = docIter->getID();
		if (docId == 0)
		{
			continue;
		}

		// The URL to download, ie the original location of the document
		string url = docIter->getOriginalLocation();
		if (url.empty() == true)
		{
			continue;
		}
#ifdef DEBUG
		cout << "mainWindow::on_refreshindex_activate: URL is " << url << endl;
#endif

		// Add this action to the queue
		DocumentInfo docInfo(docIter->getTitle(), url,
			docIter->getType(), docIter->getLanguage());
		docInfo.setTimestamp(docIter->getTimestamp());
		queue_index(docInfo, "", docId);
	}
}

//
// Index > Show Properties menu selected
//
void mainWindow::on_showfromindex_activate()
{
	vector<IndexedDocument> documentsList;
	set<string> docLabels;
	string indexName, labelName;
	DocumentInfo docInfo;
	unsigned int docId = 0;
	int width, height;
	bool matchedLabel = false, editTitle = false;

#ifdef DEBUG
	cout << "mainWindow::on_showfromindex_activate: called" << endl;
#endif
	IndexTree *pIndexTree = NULL;
	IndexPage *pIndexPage = dynamic_cast<IndexPage*>(get_current_page());
	if (pIndexPage != NULL)
	{
		indexName = locale_from_utf8(pIndexPage->getTitle());
		labelName = locale_from_utf8(pIndexPage->getLabelName());
		pIndexTree = pIndexPage->getTree();
	}

	const std::map<string, string> &indexesMap = PinotSettings::getInstance().getIndexes();
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

	XapianIndex index(mapIter->second);

	// Get the current documents selection
	if ((pIndexTree == NULL) ||
		(pIndexTree->getSelection(documentsList) == false) ||
		(documentsList.empty() == true))
	{
		// No selection
		return;
	}

	// If there's only one document selected, get its labels
	if (documentsList.size() == 1)
	{
		vector<IndexedDocument>::const_iterator docIter = documentsList.begin();

		// Get the document ID
		Url urlObj(docIter->getLocation());
		docId = (unsigned int)atoi(urlObj.getFile().c_str());

		if (index.isGood() == true)
		{
			index.getDocumentLabels(docId, docLabels);

			// Does it match the current label ?
			if ((labelName.empty() == false) &&
				(find(docLabels.begin(), docLabels.end(), labelName) != docLabels.end()))
			{
				matchedLabel = true;
			}
		}

		docInfo = DocumentInfo(docIter->getTitle(), docIter->getOriginalLocation(),
			docIter->getType(), docIter->getLanguage());
		docInfo.setTimestamp(docIter->getTimestamp());
		editTitle = true;
	}
	// Else, start with a blank list

	// Let the user set the labels
	get_size(width, height);
	propertiesDialog propertiesBox(docLabels, docInfo, editTitle);
	propertiesBox.setHeight(height / 2);
	propertiesBox.show();
	if (propertiesBox.run() != RESPONSE_OK)
	{
		return;
	}
	const set<string> &labels = propertiesBox.getLabels();

	if (index.isGood() == true)
	{
		// Now apply these labels to all the documents
		for (vector<IndexedDocument>::const_iterator docIter = documentsList.begin();
			docIter != documentsList.end(); ++docIter)
		{
			// Set the document's labels list
			index.setDocumentLabels(docIter->getID(), labels);
		}
	}

	if ((documentsList.size() == 1) &&
		(docId > 0))
	{
		bool matchesLabel = false;

		// Does the sole selected document match the current label now ?
		if ((labelName.empty() == false) &&
			(find(labels.begin(), labels.end(), labelName) != labels.end()))
		{
			matchesLabel = true;
		}

		// Was there any change ?
		if (matchesLabel != matchedLabel)
		{
			// Update this document to the index tree
			pIndexTree->setDocumentLabeledState(docId, matchesLabel);
		}

		// Did the title change ?
		string newTitle = propertiesBox.getDocumentInfo().getTitle();
		if (newTitle != docInfo.getTitle())
		{
			docInfo.setTitle(newTitle);
			
			// Update the document
			start_thread(new UpdateDocumentThread(indexName, docId, docInfo));
		}
	}
	else
	{
		if (labelName.empty() == false)
		{
			// The current label may have been applied to or removed from
			// one or more of the selected documents, so refresh the list
			start_thread(new LabelQueryThread(indexName, labelName));
		}
	}
}

//
// Index > Unindex menu selected
//
void mainWindow::on_unindex_activate()
{
	vector<IndexedDocument> documentsList;
	ustring boxTitle = _("Delete this document from the index ?");

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
		boxTitle = _("Delete these documents from the index ?");
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
	for (vector<IndexedDocument>::const_iterator docIter = documentsList.begin();
		docIter != documentsList.end(); ++docIter)
	{
		// Get the actual URL of the document to delete
		string url = docIter->getLocation();
		string liveUrl = docIter->getOriginalLocation();
		if (url.empty() == true)
		{
			continue;
		}

		unsigned int docId = docIter->getID();
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
		queue_unindex(docIdList);
	}
}

//
// Help > About menu selected
//
void mainWindow::on_about_activate()
{
	aboutDialog aboutBox;
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
	if (m_settings.addIndex(locale_from_utf8(name),
			locale_from_utf8(location)) == false)
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
		if (m_settings.removeIndex(locale_from_utf8(name)) == false)
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
// Find button click
//
void mainWindow::on_findButton_clicked()
{
	QueryProperties queryProps;

	queryProps.setName("Live query");
	// FIXME: parse the query string !
	queryProps.setAnyWords(locale_from_utf8(liveQueryEntry->get_text()));

	run_search(queryProps);
}

//
// Add query button click
//
void mainWindow::on_addQueryButton_clicked()
{
	QueryProperties queryProps = QueryProperties("", "", "", "", "");

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
		string queryName = locale_from_utf8(row[m_queryColumns.m_name]);

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

		QueryProperties queryProps = queryRow[m_queryColumns.m_properties];
		run_search(queryProps);

		// Update the Last Run column
		queryRow[m_queryColumns.m_lastRun] = TimeConverter::toTimestamp(time(NULL));
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
			pIndexPage->setFirstDocument(pIndexPage->getFirstDocument() - m_maxDocsCount);
			browse_index(indexName, pIndexPage->getFirstDocument());
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
			pIndexPage->setFirstDocument(0);
			browse_index(indexName, 0);
		}
		else if (pIndexPage->getDocumentsCount() >= pIndexPage->getFirstDocument() + m_maxDocsCount)
		{
			pIndexPage->setFirstDocument(pIndexPage->getFirstDocument() + m_maxDocsCount);
			browse_index(indexName, pIndexPage->getFirstDocument());
		}
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
	if (get_threads_count() > 0)
	{
		ustring boxTitle = _("At least one background task hasn't been completed yet. Quit now ?");
		MessageDialog msgDialog(boxTitle, false, MESSAGE_QUESTION, BUTTONS_YES_NO);
		msgDialog.set_transient_for(*this);
		msgDialog.show();
		int result = msgDialog.run();
		if (result == RESPONSE_NO)
		{
			return true;
		}

		if (m_state.readLock(6) == true)
		{
			for (set<WorkerThread *>::iterator threadIter = m_state.m_pThreads.begin();
				threadIter != m_state.m_pThreads.end(); ++threadIter)
			{
#ifdef DEBUG
				cout << "mainWindow::on_mainWindow_delete_event: stopping thread" << endl;
#endif
				// Stop all non-background threads
				if ((*threadIter)->isBackground() == false)
				{
					// FIXME: what if one thread doesn't stop ? can it corrupt anything ?
					(*threadIter)->stop();
				}
			}

			m_state.unlock();
		}
	}

	// Save the window's position and dimensions now
	// Don't worry about the gravity, it hasn't been changed
	get_position(m_settings.m_xPos, m_settings.m_yPos);
	get_size(m_settings.m_width, m_settings.m_height);
	m_settings.m_panePos = mainHpaned->get_position();

	Main::quit();
	return false;
}

//
// Returns the current page.
//
NotebookPageBox *mainWindow::get_current_page(void)
{
	NotebookPageBox *pNotebookPage = NULL;

	if (m_state.readLock(7) == true)
	{
		Widget *pPage = m_pNotebook->get_nth_page(m_pNotebook->get_current_page());
		if (pPage != NULL)
		{
			pNotebookPage = dynamic_cast<NotebookPageBox*>(pPage);
		}

		m_state.unlock();
	}

	return pNotebookPage;
}

//
// Returns the page with the given title.
//
NotebookPageBox *mainWindow::get_page(const ustring &title, NotebookPageBox::PageType type)
{
	NotebookPageBox *pNotebookPage = NULL;

	if (m_state.readLock(8) == true)
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

		m_state.unlock();
	}

	return pNotebookPage;
}

//
// Returns the number of the page with the given title.
//
int mainWindow::get_page_number(const ustring &title, NotebookPageBox::PageType type)
{
	int pageNumber = -1;

	if (m_state.readLock(9) == true)
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

		m_state.unlock();
	}

	return pageNumber;
}

//
// Queues additions to the index.
//
bool mainWindow::queue_index(const DocumentInfo &docInfo,
	const string &labelName, unsigned int docId)
{
	ActionHistory::ActionType type = ActionHistory::ACTION_INDEX;

	if (docId > 0)
	{
		// This is an update
		type = ActionHistory::ACTION_UPDATE;
	}

	if (get_threads_count() >= m_maxThreads)
	{
		ActionHistory history(m_settings.m_historyDatabase);

		string option = docInfo.getTitle();
		option += "|";
		option += Url::escapeUrl(docInfo.getLocation());
		option += "|";
		option += docInfo.getType();
		option += "|";
		option += labelName;
		if (type == ActionHistory::ACTION_UPDATE)
		{
			option += "|";
			char docIdStr[64];
			snprintf(docIdStr, 64, "%d", docId);
			option += docIdStr;
		}
#ifdef DEBUG
		cout << "mainWindow::queue_index: " << option << endl;
#endif

		// Add this to ActionHistory and return
		return history.insertItem(type, option);
	}

	if (type == ActionHistory::ACTION_UPDATE)
	{
		// Update the document
		index_document(docInfo, labelName, docId);
	}
	else
	{
		// Index the document
		index_document(docInfo, labelName);
	}

	return false;
}

//
// Queues index removals.
//
bool mainWindow::queue_unindex(set<unsigned int> &docIdList)
{
	// Delete the document(s) right away
	start_thread(new UnindexingThread(docIdList));

	return false;
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

	populate_queryTreeview();
}

//
// Runs a search
//
void mainWindow::run_search(const QueryProperties &queryProps)
{
	string querySummary = queryProps.toString();
	if (querySummary.empty() == true)
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
	vector<TreeModel::iterator> engineIters;
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
	for (vector<TreeModel::iterator>::iterator iter = engineIters.begin();
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
		start_thread(new QueryingThread(locale_from_utf8(engineName),
			locale_from_utf8(engineDisplayableName), engineOption, queryProps));
	}
}

//
// Browse an index
//
void mainWindow::browse_index(const ustring &indexName, unsigned int startDoc)
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
		// Reset variables
		pIndexPage->setDocumentsCount(0);

		// Switch to that index page
		m_pNotebook->set_current_page(get_page_number(indexName, NotebookPageBox::INDEX_PAGE));
	}

	// Spawn a new thread to browse the index
	IndexBrowserThread *pBrowseThread = new IndexBrowserThread(
		locale_from_utf8(indexName), m_maxDocsCount, startDoc);
	pBrowseThread->getUpdateSignal().connect(SigC::slot(*this,
		&mainWindow::on_message_indexupdate));
	start_thread(pBrowseThread);
}

//
// Index (or update) a document
//
void mainWindow::index_document(const DocumentInfo &docInfo,
	const string &labelName, unsigned int docId)
{
	Url urlObj(docInfo.getLocation());
	string indexLocation = m_settings.m_indexLocation;

	// If the document is mail, we need to check My Email
	if (urlObj.getProtocol() == "mailbox")
	{
		indexLocation = m_settings.m_mailIndexLocation;
	}

	XapianIndex index(indexLocation);

	// Is it an update ?
	if (docId > 0)
	{
		// Yes, it is
		start_thread(new IndexingThread(docInfo, docId));
	}
	else
	{
		string url(docInfo.getLocation());
		bool isNewDocument = false;

		// No : see if the document is already indexed
		// or is being indexed
		if (index.isGood() == true)
		{
			docId = index.hasDocument(url);
		}
		if ((docId == 0) &&
			(m_state.writeLock(10) == true))
		{
			if (m_state.m_beingIndexed.find(url) == m_state.m_beingIndexed.end())
			{
				m_state.m_beingIndexed.insert(url);
				isNewDocument = true;
			}

			m_state.unlock();
		}

		if (isNewDocument == true)
		{
			// This is a new document
			start_thread(new IndexingThread(docInfo, labelName));
		}
		else
		{
			ustring status = url;
			status += " ";
			status += _("is already indexed or is being indexed");
			set_status(status);
		}
	}

	// If the document is indexed, we may have to update its labels
	if ((docId > 0) &&
		(labelName.empty() == false))
	{
		set<string> docLabels;

		// Add this new label
#ifdef DEBUG
		cout << "mainWindow::index_document: applying label " << labelName << " to document " << docId << endl;
#endif
		docLabels.insert(labelName);
		index.setDocumentLabels(docId, docLabels, false);
	}
}

//
// View a document
//
bool mainWindow::view_document(const string &url, bool internalViewerOnly)
{
	if (url.empty() == true)
	{
		set_status(_("No URL to browse"));
		return false;
	}

	// Is browsing enabled ?
	if ((internalViewerOnly == false) &&
		(m_settings.m_browseResults == true))
	{
		// Point user-defined browser to that URL
		if (m_settings.m_browserCommand.empty() == true)
		{
			set_status(_("No browser configured to view results"));
			return false;
		}

		string shellCommand = locale_from_utf8(m_settings.m_browserCommand);
		// FIXME: do substitutions
		shellCommand += " \"";
		shellCommand += url;
		shellCommand += "\" &";
		if ((url.empty() == true) ||
			(system(shellCommand.c_str()) == -1))
		{
			ustring status = _("Couldn't browse URL:");
			status += " ";
			status += Glib::strerror(errno);
			set_status(status);
		}
	}
	else
	{
		Url urlObj(url);

		// FIXME: there should be a way to know which protocols can be viewed/indexed
		if (urlObj.getProtocol() == "mailbox")
		{
			// Get that message
			start_thread(new DownloadingThread(url, false));
		}
		else
		{
			ViewPage *pViewPage = NULL;
			ustring viewName = _("View");
			int pageNum = -1;

			// Is there already a view page ?
			pViewPage = dynamic_cast<ViewPage*>(get_page(viewName, NotebookPageBox::VIEW_PAGE));
			if (pViewPage != NULL)
			{
				pageNum = get_page_number(viewName, NotebookPageBox::VIEW_PAGE);
				// The page may be hidden
				pViewPage->show();
				// FIXME: reorder pages
			}
			else
			{
				NotebookTabBox *pTab = manage(new NotebookTabBox(viewName,
					NotebookPageBox::VIEW_PAGE));
				pTab->getCloseSignal().connect(
					SigC::slot(*this, &mainWindow::on_close_page));

				// Position everything
				pViewPage = manage(new ViewPage(viewName, m_pHtmlView, m_settings));

				// Append the page
				if (m_state.writeLock(1) == true)
				{
					pageNum = m_pNotebook->append_page(*pViewPage, *pTab);
					m_pNotebook->pages().back().set_tab_label_packing(false, true, Gtk::PACK_START);

					m_state.unlock();
				}
			}

			if (pageNum >= 0)
			{
				// Display the URL
				m_pNotebook->set_current_page(pageNum);
				if (m_pHtmlView->renderUrl(url) == true)
				{
					//viewstop1->set_sensitive(true);
				}
				set_status(locale_to_utf8(m_pHtmlView->getLocation()));
			}
		}
	}

	// Record this into the history
	ViewHistory viewHistory(m_settings.m_historyDatabase);
	if (viewHistory.hasItem(url) == false)
	{
		viewHistory.insertItem(url);
	}

	return true;
}

//
// Start of worker thread
//
void mainWindow::start_thread(WorkerThread *pNewThread, bool inBackground)
{
	static unsigned int nextId = 1;
	bool insertedThread = false;

	if (pNewThread == NULL)
	{
		return;
	}

	pNewThread->setId(nextId);
	// Connect to the finished signal
	pNewThread->getFinishedSignal().connect(SigC::slot(*this,
		&mainWindow::on_thread_end));

	if (m_state.writeLock(11) == true)
	{
		pair<set<WorkerThread *>::iterator, bool> insertPair = m_state.m_pThreads.insert(pNewThread);
		insertedThread = insertPair.second;

		m_state.unlock();
	}

	// Was it inserted ?
	if (insertedThread == false)
	{
		// No, it wasn't : delete the object and return
		cerr << "mainWindow::start_thread: couldn't start "
			<< pNewThread->getType() << " " << pNewThread->getId() << endl;
		delete pNewThread;

		return;
	}

	// Start the thread
#ifdef DEBUG
	cout << "mainWindow::start_thread: start of " << pNewThread->getType()
		<< " " << pNewThread->getId() << endl;
#endif
	if (inBackground == true)
	{
		pNewThread->inBackground();
		++m_state.m_backgroundThreads;
	}
	pNewThread->start();

	if (inBackground == false)
	{
		// Enable the activity progress bar
		m_timeoutConnection.block();
		m_timeoutConnection.disconnect();
		m_timeoutConnection = Glib::signal_timeout().connect(SigC::slot(*this,
			&mainWindow::on_activity_timeout), 1000);
		m_timeoutConnection.unblock();
		// Update the status
		update_threads_status();
	}
	++nextId;
}

//
// Checks the queue and runs the oldest action if possible.
//
bool mainWindow::check_queue(void)
{
#ifdef DEBUG
	cout << "mainWindow::check_queue: called" << endl;
#endif
	if (get_threads_count() >= m_maxThreads)
	{
#ifdef DEBUG
		cout << "mainWindow::check_queue: too many threads" << endl;
#endif
		return false;
	}

	ActionHistory history(m_settings.m_historyDatabase);
	ActionHistory::ActionType type;
	string option;

	if (history.deleteOldestItem(type, option) == false)
	{
#ifdef DEBUG
		cout << "mainWindow::check_queue: found no action" << endl;
#endif
		return false;
	}

	if (type == ActionHistory::ACTION_INDEX)
	{
		string::size_type lastPos = 0, pos = option.find_first_of("|");
		if (pos != string::npos)
		{
			string title = option.substr(lastPos, pos - lastPos);

			lastPos = pos + 1;
			pos = option.find_first_of("|", lastPos);
			if (pos != string::npos)
			{
				string url = option.substr(lastPos, pos - lastPos);

				lastPos = pos + 1;
				pos = option.find_first_of("|", lastPos);
				if (pos != string::npos)
				{
					string type = option.substr(lastPos, pos - lastPos);

					string labelName = option.substr(pos + 1);

					// Index the document
					index_document(DocumentInfo(title, Url::unescapeUrl(url), type, ""),
						labelName);
				}
			}
		}
	}
	else if (type == ActionHistory::ACTION_UPDATE)
	{
		string::size_type lastPos = 0, pos = option.find_first_of("|");
		if (pos != string::npos)
		{
			string title = option.substr(lastPos, pos - lastPos);

			lastPos = pos + 1;
			pos = option.find_first_of("|", lastPos);
			if (pos != string::npos)
			{
				string url = option.substr(lastPos, pos - lastPos);

				lastPos = pos + 1;
				pos = option.find_first_of("|", lastPos);
				if (pos != string::npos)
				{
					string type = option.substr(lastPos, pos - lastPos);

					lastPos = pos + 1;
					pos = option.find_first_of("|", lastPos);
					if (pos != string::npos)
					{
						string labelName = option.substr(lastPos, pos - lastPos);

						unsigned int docId = (unsigned int)atoi(option.substr(pos + 1).c_str());

						// Update the document
						index_document(DocumentInfo(title, Url::unescapeUrl(url), type, ""),
							labelName, docId);
					}
				}
			}
		}
	}

	return true;
}

//
// Returns the number of non-background threads.
//
unsigned int mainWindow::get_threads_count(void)
{
	int count = 0;

	if (m_state.readLock(12) == true)
	{
		count = m_state.m_pThreads.size() - m_state.m_backgroundThreads;
		m_state.unlock();
	}
#ifdef DEBUG
	cout << "mainWindow::get_threads_count: " << count << " threads left" << endl;
#endif

	// A negative count would mean that a background thread returned
	// without the main thread knowing about it
	return (unsigned int)max(count , 0);
}

//
// Updates the threads status text.
//
void mainWindow::update_threads_status(void)
{
	ustring text;
	unsigned int threads = get_threads_count();

	// Update the threads status text for the next call to set_status()
	if (threads > 0)
	{
		char countStr[64];
		snprintf(countStr, 64, "%d", threads);
		text = countStr;
		text += " ";
		text += _("thread(s)");
		text += " - ";
		m_threadStatusText = text;
	}
	else
	{
		m_threadStatusText = "";
	}
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
	
	// Pop the previous message
	mainStatusbar->pop();
	// Append the new message to the threads status text
	ustring newText = m_threadStatusText;
	newText += text;
	// Push
	mainStatusbar->push(newText);
}
