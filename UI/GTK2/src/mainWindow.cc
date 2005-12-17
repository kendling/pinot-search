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
	m_currentLabelPos(0),
	m_currentLabelName(_("None")),
	m_currentIndexName(_("My Documents")),
	m_indexDocsCount(0),
	m_startDoc(0),
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

unsigned int mainWindow::InternalState::getCurrentLabel(string &labelName)
{
	if (readLock(1) == true)
	{
		unsigned int labelPos = m_currentLabelPos;
		labelName = m_currentLabelName;
		unlock();

		return labelPos;
	}
	
	return 0;
}

void mainWindow::InternalState::setCurrentLabel(unsigned int labelPos, const string &labelName)
{
	if (writeLock(1) == true)
	{
		m_currentLabelPos = labelPos;
		m_currentLabelName = labelName;
		unlock();
	}
}

Glib::ustring mainWindow::InternalState::getCurrentIndex(void)
{
	ustring indexName;

	if (readLock(2) == true)
	{
		indexName = m_currentIndexName;
		unlock();
	}

	return indexName;
}

void mainWindow::InternalState::setCurrentIndex(const Glib::ustring &indexName)
{
	if (writeLock(2) == true)
	{
		m_currentIndexName = indexName;
		unlock();
	}
}

//
// Constructor
//
mainWindow::mainWindow() :
	m_settings(PinotSettings::getInstance()), mainWindow_glade(),
	m_pEnginesTree(NULL),
	m_pResultsTree(NULL),
	m_pIndexTree(NULL),
	m_pLabelsMenu(NULL)
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

	// Position the results tree
	m_pResultsTree = manage(new ResultsTree(resultsVbox, resultsMenuitem->get_submenu(), m_settings));
	// Connect to the "changed" signal
	m_pResultsTree->get_selection()->signal_changed().connect(SigC::slot(*this,
		&mainWindow::on_resultsTreeviewSelection_changed));

	// Position the index tree
	m_pIndexTree = manage(new IndexTree(indexVbox, indexMenuitem->get_submenu(), m_settings));
	// Connect to the "changed" signal
	m_pIndexTree->get_selection()->signal_changed().connect(SigC::slot(*this,
		&mainWindow::on_indexTreeviewSelection_changed));
	// Connect to the edit document signal
	m_pIndexTree->getEditDocumentSignal().connect(SigC::slot(*this,
		&mainWindow::on_showfromindex_activate));

	// Associate the columns model to the index combo
	m_refIndexNameTree = ListStore::create(m_indexNameColumns);
	indexCombobox->set_model(m_refIndexNameTree);
	indexCombobox->pack_start(m_indexNameColumns.m_name);
	// Populate the index combo
	populate_indexCombobox();

	// Populate the label menu
	populate_labelMenu();

	// Add an HTML renderer in the View tab
	m_pHtmlView = new HtmlView(viewVbox, NULL);
	if (m_settings.m_browseResults == true)
	{
		// Hide this tab
		Widget *pPage = mainNotebook->get_nth_page(2);
		if (pPage != NULL)
		{
			pPage->hide();
		}
	}
	else
	{
		view_document("file:///usr/share/pinot/index.html", true);
	}

	// Gray out menu items
	editQueryButton->set_sensitive(false);
	removeQueryButton->set_sensitive(false);
	findQueryButton->set_sensitive(false);
	clearresults1->set_sensitive(false);
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
	showlabels1->set_sensitive(false);
	viewfromindex1->set_sensitive(false);
	refreshindex1->set_sensitive(false);
	showfromindex1->set_sensitive(false);
	unindex1->set_sensitive(false);
	//viewstop1->set_sensitive(false);
	// ...and buttons
	removeIndexButton->set_sensitive(false);
	indexBackButton->set_sensitive(false);
	indexForwardButton->set_sensitive(false);

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

	// List the documents index
	browse_index();
	// There might be queued actions
	check_queue();

	// Now we are ready
	set_status(_("Ready"));
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

	// This is a hack to avoid segfaults when the View tab hasn't been made visible
	Widget *pPage = mainNotebook->get_nth_page(2);
	if (pPage != NULL)
	{
		pPage->show();
	}
	mainNotebook->set_current_page(2);
	// Stop if we were loading a page
	m_pHtmlView->stop();
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
// Populate the indexes list
//
void mainWindow::populate_indexCombobox()
{
	TreeModel::iterator iter;
	TreeModel::Row row;

	const std::map<string, string> &indexes = m_settings.getIndexes();
#ifdef DEBUG
	cout << "mainWindow::populate_indexCombobox: " << indexes.size() << " indexes" << endl;
#endif
	m_refIndexNameTree->clear();
	std::map<string, string>::const_iterator indexIter = indexes.begin();
	for (; indexIter != indexes.end(); ++indexIter)
	{
		string indexName = indexIter->first;

		if (m_settings.isInternalIndex(indexName) == true)
		{
			// Add this index
			iter = m_refIndexNameTree->append();
			row = *iter;
			row[m_indexNameColumns.m_name] = to_utf8(indexName);
#ifdef DEBUG
			cout << "mainWindow::populate_indexCombobox: added " << indexName << endl;
#endif
		}
	}

	indexCombobox->set_active(0);
}

//
// Populate the labels menu
//
void mainWindow::populate_labelMenu()
{
	RadioMenuItem::Group labelsGroup;
	RadioMenuItem *firstLabelMenuItem = NULL;
	RadioMenuItem *labelMenuItem = NULL;

#ifdef DEBUG
	cout << "mainWindow::populate_labels_menu: called" << endl;
#endif
	if (m_pLabelsMenu == NULL)
	{
		m_pLabelsMenu = new Menu();
		showlabels1->set_submenu(*m_pLabelsMenu);
	}
	else
	{
		// Clear the submenu
		m_pLabelsMenu->items().clear();
	}

	SigC::Slot1<void, unsigned int> labelsSlot = SigC::slot(*this, &mainWindow::on_labelMenu_changed);
	unsigned int count = 1;
                                                                                                                                                             
	// Initialize the submenu
	m_pLabelsMenu->items().push_back(Menu_Helpers::RadioMenuElem(labelsGroup, _("None")));
	firstLabelMenuItem = labelMenuItem = (RadioMenuItem*)&m_pLabelsMenu->items().back();
	// Bind the callback's parameter to the menuitem's position in the submenu
	SigC::Slot0<void> labelsSlot0 = sigc::bind(labelsSlot, 0);
	labelMenuItem->signal_activate().connect(labelsSlot0);
	for (set<PinotSettings::Label>::const_iterator iter = m_settings.m_labels.begin();
		iter != m_settings.m_labels.end(); ++iter)
	{
		m_pLabelsMenu->items().push_back(Menu_Helpers::RadioMenuElem(labelsGroup, iter->m_name));
		labelMenuItem = (RadioMenuItem*)&m_pLabelsMenu->items().back();
		SigC::Slot0<void> labelsSlot0 = sigc::bind(labelsSlot, count);
		labelMenuItem->signal_activate().connect(labelsSlot0);
		++count;
	}

	// Activate the first menuitem
	firstLabelMenuItem->set_active(true);
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
void mainWindow::on_resultsTreeviewSelection_changed()
{
	if (m_pResultsTree->onSelectionChanged() == true)
	{
		ustring url = m_pResultsTree->getFirstSelectionURL();
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
void mainWindow::on_indexTreeviewSelection_changed()
{
	if (m_pIndexTree->onSelectionChanged() == true)
	{
		bool isDocumentsIndex = true;
		bool canViewDocument = true;

		// Enable these menu items, unless the index is not the documents index
		ustring currentIndexName = m_state.getCurrentIndex();
		if (currentIndexName != _("My Documents"))
		{
			isDocumentsIndex = false;
		}
		if ((currentIndexName == _("My Email")) &&
			(m_settings.m_browseResults == true))
		{
			canViewDocument = false;
		}
		viewfromindex1->set_sensitive(canViewDocument);
		refreshindex1->set_sensitive(isDocumentsIndex);
		showfromindex1->set_sensitive(true);
		unindex1->set_sensitive(isDocumentsIndex);

		// Show the URL in the status bar
		ustring statusText = _("Document location is");
		statusText += " ";
		statusText += m_pIndexTree->getFirstSelectionURL();
		set_status(statusText, true);
	}
	else
	{
		// No, disable these
		viewfromindex1->set_sensitive(false);
		refreshindex1->set_sensitive(false);
		showfromindex1->set_sensitive(false);
		unindex1->set_sensitive(false);
	}
}

//
// Index > Show Labels menu selected
//
void mainWindow::on_labelMenu_changed(unsigned int pos)
{
	string currentLabelName;
	unsigned int currentLabelPos;

	// Since we listen for signal_activate(), this handler gets called when the
	// current item is unselected and when the new one is selected
	currentLabelPos = m_state.getCurrentLabel(currentLabelName);
	if (currentLabelPos == pos)
	{
		// Ignore unselections
		return;
	}
#ifdef DEBUG
	cout << "mainWindow::on_labelMenu_changed: called on " << pos << endl;
#endif
	currentLabelPos = pos;
	if (currentLabelPos == 0)
	{
		// User selected no label
		currentLabelName = _("None");
		m_state.setCurrentLabel(currentLabelPos, currentLabelName);
		m_pIndexTree->setCurrentLabelColour(0, 0, 0, false);
		set_status(_("No labels"));
		return;
	}

	// Get the actual label from the settings object
	unsigned int labelNum = 1;
	for (set<PinotSettings::Label>::iterator labelIter = m_settings.m_labels.begin();
		labelIter != m_settings.m_labels.end(); ++labelIter)
	{
		if (labelNum == pos)
		{
			// That's the one
			currentLabelName = locale_from_utf8(labelIter->m_name);
			m_state.setCurrentLabel(currentLabelPos, currentLabelName);
#ifdef DEBUG
			cout << "mainWindow::on_labelMenu_changed: label is " << currentLabelName << endl;
#endif
			// Switch temporarily to no label
			m_pIndexTree->setCurrentLabelColour(0, 0, 0, false);

			// Get the documents that match this label
			start_thread(new LabelQueryThread(m_state.getCurrentIndex(), currentLabelName));
			break;
		}
		++labelNum;
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
		bool enableIndexOps = false;

		IndexBrowserThread *pBrowseThread = dynamic_cast<IndexBrowserThread *>(pThread);
		if (pBrowseThread == NULL)
		{
			delete pThread;
			return;
		}

		m_state.m_indexDocsCount = pBrowseThread->getDocumentsCount();
		count = m_pIndexTree->getRowsCount();

		status = _("Showing");
		status += " ";
		snprintf(docsCountStr, 64, "%u", count);
		status += docsCountStr;
		status += " ";
		status += _("off");
		status += " ";
		snprintf(docsCountStr, 64, "%u", m_state.m_indexDocsCount);
		status += docsCountStr;
		status += " ";
		status += _("documents from");
		status += " ";
		status += m_state.getCurrentIndex();
		set_status(status);

		if (m_state.m_indexDocsCount > 0)
		{
			// Switch to the index page
			mainNotebook->set_current_page(1);
			// FIXME: not sure why, but this helps with refreshing the tree
			m_pIndexTree->columns_autosize();
			enableIndexOps = true;
		}
		showlabels1->set_sensitive(enableIndexOps);
#ifdef DEBUG
		cout << "mainWindow::on_thread_end: " << m_state.m_indexDocsCount
			<< " documents, starting at " << m_state.m_startDoc << endl;
#endif
		if (m_state.m_startDoc >= m_maxDocsCount)
		{
			indexBackButton->set_sensitive(true);
		}
		else
		{
			indexBackButton->set_sensitive(false);
		}
		if (m_state.m_indexDocsCount >= m_state.m_startDoc + m_maxDocsCount)
		{
			indexForwardButton->set_sensitive(true);
		}
		else
		{
			indexForwardButton->set_sensitive(false);
		}
		m_state.m_browsingIndex = false;
	}
	else if (type == "QueryingThread")
	{
		std::map<string, TreeModel::iterator> updatedGroups;
		unsigned int count = 0;
		ResultsModelColumns::ResultType rootType;
		bool mergeDuplicates = false;

		QueryingThread *pQueryThread = dynamic_cast<QueryingThread *>(pThread);
		if (pQueryThread == NULL)
		{
			delete pThread;
			return;
		}

		QueryProperties queryProps = pQueryThread->getQuery();
		string queryName = queryProps.getName();
		string engineName = pQueryThread->getEngineName();

		status = _("Query");
		status += " ";
		status += to_utf8(queryName);
		status += " ";
		status += _("on");
		status += " ";
		status += to_utf8(engineName);
		status += " ";
		status += _("ended");
		set_status(status);

		// Switch to the results page
		mainNotebook->set_current_page(0);

		// Add these results to the tree
		const vector<Result> &resultsList = pQueryThread->getResults();
		if (m_pResultsTree->addResults(queryProps, engineName,
			resultsList, searchenginegroup1->get_active()) == true)
		{
#ifdef DEBUG
			cout << "mainWindow::on_thread_end: added results" << endl;
#endif
			// Enable results clearing
			clearresults1->set_sensitive(true);

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
	}
	else if (type == "LabelQueryThread")
	{
		string currentLabelName;
		unsigned int currentLabelPos = 0;

		LabelQueryThread *pLabelQueryThread = dynamic_cast<LabelQueryThread *>(pThread);
		if (pLabelQueryThread == NULL)
		{
			delete pThread;
			return;
		}

		// Don't bother if the index is not being listed, or if the user
		// changed the label in the meantime
		currentLabelPos = m_state.getCurrentLabel(currentLabelName);
		if ((m_pIndexTree->isEmpty() == false) &&
			(currentLabelPos > 0) &&
			(currentLabelName == pLabelQueryThread->getLabelName()))
		{
			const set<unsigned int> &documentsList = pLabelQueryThread->getDocumentsList();
			char docsCountStr[64];
			unsigned int docsCount = documentsList.size();
			unsigned int labelNum = 1;

			status = _("Label");
			status += " ";
			status += to_utf8(pLabelQueryThread->getLabelName());
			status += " ";
			status += _("matches");
			status += " ";
			snprintf(docsCountStr, 64, "%u", docsCount);
			status += docsCountStr;
			status += " ";
			status += _("document(s)");
			set_status(status);

#ifdef DEBUG
			cout << "mainWindow::on_thread_end: current label is " << currentLabelName << endl;
#endif
			// Get the actual label colour from the settings
			for (set<PinotSettings::Label>::iterator labelIter = m_settings.m_labels.begin();
				labelIter != m_settings.m_labels.end(); ++labelIter)
			{
#ifdef DEBUG
				cout << "mainWindow::on_thread_end: looking at label "
					<< labelIter->m_name << ", position " << labelNum << endl;
#endif
				if (labelNum == currentLabelPos)
				{
					// Display the selected label's colour in the index tree
					m_pIndexTree->setCurrentLabelColour(labelIter->m_red, labelIter->m_green, labelIter->m_blue);
					m_pIndexTree->setLabel(documentsList);

					// Switch to the index page
					mainNotebook->set_current_page(1);
					break;
				}
				++labelNum;
			}
		}
	}
	else if (type == "LabelUpdateThread")
	{
		LabelQueryThread *pLabelQueryThread = dynamic_cast<LabelQueryThread *>(pThread);
		if (pLabelQueryThread == NULL)
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
					// Display the URL in the View tab
					mainNotebook->set_current_page(2);
					if (m_pHtmlView->renderData(pData, dataLength, url) == true)
					{
						//viewstop1->set_sensitive(true);
					}
					set_status(locale_to_utf8(url));
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
			if (m_state.writeLock(3) == true)
			{
				set<string>::iterator urlIter = m_state.m_beingIndexed.find(url);
				if (urlIter != m_state.m_beingIndexed.end())
				{
					m_state.m_beingIndexed.erase(urlIter);
				}

				m_state.unlock();
			}

			// Was the current label applied to that document ?
			string currentLabelName;
			if ((m_state.getCurrentLabel(currentLabelName) > 0) &&
				(pIndexThread->getLabelName() == currentLabelName))
			{
				labeled = true;
			}

			// Is the index still being shown ?
			if (m_state.getCurrentIndex() == _("My Documents"))
			{
				XapianIndex index(m_settings.m_indexLocation);

				if (index.isGood() == true)
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
							m_pIndexTree->appendDocument(indexedDoc, labeled);
						}
					}
				}

				showlabels1->set_sensitive(true);
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

		m_pIndexTree->setDocumentTitle(pUpdateThread->getDocumentID(),
			pUpdateThread->getDocumentInfo().getTitle());

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
	if (indexName != locale_from_utf8(m_state.getCurrentIndex()))
	{
		// Ignore
#ifdef DEBUG
		cout << "mainWindow::on_message_indexupdate: " << indexName << " not current" << endl;
#endif
		return;
	}

	// Is the last page being displayed ?
	if (m_state.m_startDoc + m_maxDocsCount < m_state.m_indexDocsCount)
	{
		// No, so we can't add a new entry for that document
		// Increment the count
		++m_state.m_indexDocsCount;
		// ...and make sure the user can display that last page
		indexForwardButton->set_sensitive(true);
		return;
	}

	const std::map<string, string> &indexesMap = PinotSettings::getInstance().getIndexes();
	std::map<string, string>::const_iterator mapIter = indexesMap.find(indexName);
	if (mapIter == indexesMap.end())
	{
		return;
	}

	// Add a row
	if (m_pIndexTree->appendDocument(docInfo, true) == true)
	{
#ifdef DEBUG
		cout << "mainWindow::on_message_indexupdate: added document to index list" << endl;
#endif
		++m_state.m_indexDocsCount;
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

	// Synchronize the labels list with the new settings
	m_pIndexTree->setCurrentLabelColour(0, 0, 0, false);
	populate_labelMenu();

	// Do the changes affect the View tab ?
	if (useExternalBrowser != m_settings.m_browseResults)
	{
		int nCurrentPage = mainNotebook->get_current_page();
		Widget *pPage = mainNotebook->get_nth_page(2);
		if (pPage != NULL)
		{
			// Hide or show ?
			if (m_settings.m_browseResults == true)
			{
				pPage->hide();
			}
			else
			{
				pPage->show();

				// Make sure we show the same tab
				mainNotebook->set_current_page(nCurrentPage);
			}
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
	else if (m_pResultsTree->is_focus() == true)
	{
		vector<Result> resultsList;
		bool firstItem = true;

#ifdef DEBUG
		cout << "mainWindow::on_copy_activate: results tree" << endl;
#endif
		// Get the current results selection
		m_pResultsTree->getSelection(resultsList);
	
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
	else if (m_pIndexTree->is_focus() == true)
	{
		vector<IndexedDocument> documentsList;
		bool firstItem = true;

#ifdef DEBUG
		cout << "mainWindow::on_copy_activate: index tree" << endl;
#endif
		// Get the current documents selection
		m_pIndexTree->getSelection(documentsList);
	
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
	else
	{
		// Only rows from the query, results and index trees can be copied
#ifdef DEBUG
		cout << "mainWindow::on_copy_activate: other" << endl;
#endif
		return;
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
	else if (m_pResultsTree->is_focus() == true)
	{
#ifdef DEBUG
		cout << "mainWindow::on_delete_activate: results tree" << endl;
#endif
		if (m_pResultsTree->deleteSelection() == true)
		{
			// The results tree is now empty
			clearresults1->set_sensitive(false);
		}
	}
	// Nothing else can be deleted
}

//
// Results > Clear menu selected
//
void mainWindow::on_clearresults_activate()
{
	m_pResultsTree->clear();
	clearresults1->set_sensitive(false);
}

//
// Results > Show Extract menu selected
//
void mainWindow::on_showextract_activate()
{
#ifdef DEBUG
	cout << "mainWindow::on_showextract_activate: called" << endl;
#endif
	m_pResultsTree->showExtract(showextract1->get_active());
}

//
// Results > Group menu selected
//
void mainWindow::on_groupresults_activate()
{
	ResultsModelColumns::ResultType currentType, newType;

#ifdef DEBUG
	cout << "mainWindow::on_groupresults_activate: called" << endl;
#endif
	// What's the new grouping criteria ?
	m_pResultsTree->regroupResults(searchenginegroup1->get_active());
}

//
// Results > View menu selected
//
void mainWindow::on_viewresults_activate()
{
	ustring url = m_pResultsTree->getFirstSelectionURL();
	if (view_document(locale_from_utf8(url)) == true)
	{
		// We can update the row right now
		m_pResultsTree->setFirstSelectionViewedState(true);
	}
}

//
// Results > View Cache menu selected
//
void mainWindow::on_viewcache_activate()
{
	ustring url = m_pResultsTree->getFirstSelectionURL();

	start_thread(new DownloadingThread(url, true));

	// Update the row now, even though the cached page may not be retrieved
	m_pResultsTree->setFirstSelectionViewedState(true);
}

//
// Results > Index menu selected
//
void mainWindow::on_indexresults_activate()
{
	// Make sure this has been configured
	if (m_settings.m_indexLocation.empty() == true)
	{
		set_status(_("Please set a location for the index first"));
		return;
	}

	vector<Result> resultsList;
	m_pResultsTree->getSelection(resultsList);

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
	// Let the signal handler deal with mporting stuff
}

//
// Index > View menu selected
//
void mainWindow::on_viewfromindex_activate()
{
	// View the first document, don't bother about the rest
	ustring url = m_pIndexTree->getFirstSelectionLiveURL();
	view_document(locale_from_utf8(url));
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
	if ((m_pIndexTree->getSelection(documentsList) == false) ||
		(documentsList.empty() == true))
	{
		// No selection
		return;
	}

	for (vector<IndexedDocument>::const_iterator docIter = documentsList.begin();
		docIter != documentsList.end(); ++docIter)
	{
		// The URL to download, ie the original location of the document
		string url = docIter->getOriginalLocation();
		if (url.empty() == true)
		{
			continue;
		}

		// The document ID
		unsigned int docId = docIter->getID();
		if (docId == 0)
		{
			continue;
		}

		// The title
		string title = docIter->getTitle();
#ifdef DEBUG
		cout << "mainWindow::on_refreshindex_activate: URL is " << url << endl;
#endif

		// Add this action to the queue
		queue_index(*docIter, "", docId);
	}
}

//
// Index > Show Properties menu selected
//
void mainWindow::on_showfromindex_activate()
{
	vector<IndexedDocument> documentsList;
	set<string> docLabels;
	DocumentInfo docInfo;
	unsigned int docId = 0;
	int width, height;
	bool editTitle = false;

	const std::map<string, string> &indexesMap = PinotSettings::getInstance().getIndexes();
	std::map<string, string>::const_iterator mapIter = indexesMap.find(m_state.getCurrentIndex());	
	if (mapIter == indexesMap.end())
	{
		ustring statusText = _("Index");
		statusText += " ";
		statusText += m_state.getCurrentIndex();
		statusText += " ";
		statusText += _("doesn't exist");
		set_status(statusText);
		return;
	}

	XapianIndex index(mapIter->second);

	// Get the current documents selection
	if ((m_pIndexTree->getSelection(documentsList) == false) ||
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
		string newTitle = propertiesBox.getDocumentInfo().getTitle();
		if (newTitle != docInfo.getTitle())
		{
			docInfo.setTitle(newTitle);
			
			// Update the document's title
			start_thread(new UpdateDocumentThread(m_state.getCurrentIndex(), docId, docInfo));
		}
	}
	else
	{
		string currentLabelName;

		if (m_state.getCurrentLabel(currentLabelName) > 0)
		{
			// The current label may have been applied to or removed from
			// one or more of the selected documents, so refresh the list
			start_thread(new LabelQueryThread(m_state.getCurrentIndex(), currentLabelName));
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

	// Get the current documents selection
	if ((m_pIndexTree->getSelection(documentsList) == false) ||
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
	if (m_pIndexTree->deleteSelection() == true)
	{
		// The index tree is now empty
		showlabels1->set_sensitive(false);
	}

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
// Index list mouse click
//
void mainWindow::on_indexCombobox_changed()
{
	TreeModel::iterator indexIter = indexCombobox->get_active();
	if (indexIter)
	{
		TreeModel::Row row = *indexIter;

		ustring indexName = row[m_indexNameColumns.m_name];
#ifdef DEBUG
		cout << "mainWindow::on_indexCombobox_changed: current index now " << indexName << endl;
#endif
		ustring currentIndexName = m_state.getCurrentIndex();
		m_state.setCurrentIndex(indexName);
		if ((currentIndexName.empty() == false) &&
			(currentIndexName != indexName))
		{
			// Force a refresh if the selected index has changed
			m_state.m_startDoc = m_state.m_indexDocsCount = 0;
			on_indexForwardButton_clicked();
		}
	}
}

//
// Index back button click
//
void mainWindow::on_indexBackButton_clicked()
{
	if (m_state.m_startDoc >= m_maxDocsCount)
	{
		m_state.m_startDoc -= m_maxDocsCount;
		browse_index(m_state.m_startDoc);
	}
}

//
// Index forward button click
//
void mainWindow::on_indexForwardButton_clicked()
{
	if (m_state.m_indexDocsCount == 0)
	{
		m_state.m_startDoc = 0;
		browse_index(m_state.m_startDoc);
	}
	else if (m_state.m_indexDocsCount >= m_state.m_startDoc + m_maxDocsCount)
	{
		m_state.m_startDoc += m_maxDocsCount;
		browse_index(m_state.m_startDoc);
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

		if (m_state.readLock(4) == true)
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
void mainWindow::browse_index(unsigned int startDoc)
{
	bool enableIndexOps = false;

	// Rudimentary lock
	if (m_state.m_browsingIndex == true)
	{
		return;
	}
	m_state.m_browsingIndex = true;

	// Remove existing rows in the index tree
	m_pIndexTree->clear();
	// Disable this
	showlabels1->set_sensitive(false);
	// Reset variables
	m_state.m_indexDocsCount = 0;

	if (m_state.getCurrentIndex() == _("My Documents"))
	{
		enableIndexOps = true;
	}
	import1->set_sensitive(enableIndexOps);

	// Spawn a new thread to browse the index
#ifdef DEBUG
	cout << "mainWindow::browse_index: indexing " << m_state.getCurrentIndex() << endl;
#endif
	IndexBrowserThread *pBrowseThread = new IndexBrowserThread(
		locale_from_utf8(m_state.getCurrentIndex()), m_maxDocsCount, startDoc);
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
			(m_state.writeLock(4) == true))
		{
			if (m_state.m_beingIndexed.find(url) == m_state.m_beingIndexed.end())
			{
				m_state.m_beingIndexed.insert(url);
				isNewDocument = true;
			}
#ifdef DEBUG
			else cout << "mainWindow::index_document: already indexed " << url << endl;
#endif

			m_state.unlock();
		}

		if (isNewDocument == true)
		{
			// This is a new document
			start_thread(new IndexingThread(docInfo, labelName));
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
#ifdef DEBUG
	cout << "mainWindow::view_document: URL is " << url << endl;
#endif

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
			status == " ";
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
			// Display the URL in the View tab
			mainNotebook->set_current_page(2);
			if (m_pHtmlView->renderUrl(url) == true)
			{
				//viewstop1->set_sensitive(true);
			}
			set_status(locale_to_utf8(m_pHtmlView->getLocation()));
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

	if (m_state.writeLock(5) == true)
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

	if (m_state.readLock(5) == true)
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
