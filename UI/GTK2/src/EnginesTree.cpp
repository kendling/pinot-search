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

#include <iostream>
#include <gtkmm/alignment.h>
#include <gtkmm/box.h>
#include <gtkmm/buttonbox.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/stock.h>
#include <gtkmm/textbuffer.h>

#include "Url.h"
#include "QueryHistory.h"
#include "ViewHistory.h"
#include "config.h"
#include "NLS.h"
#include "PinotSettings.h"
#include "PinotUtils.h"
#include "EnginesTree.h"

using namespace std;
using namespace SigC;
using namespace Glib;
using namespace Gdk;
using namespace Gtk;

EnginesTree::EnginesTree(VBox *enginesVbox, PinotSettings &settings) :
	TreeView(),
	m_settings(settings)
{
	ScrolledWindow *enginesScrolledwindow = manage(new ScrolledWindow());

	// This is the actual engines tree
	set_events(Gdk::BUTTON_PRESS_MASK);
	set_flags(CAN_FOCUS);
	set_headers_clickable(true);
	set_headers_visible(true);
	set_reorderable(false);
	set_enable_search(false);
	get_selection()->set_mode(SELECTION_MULTIPLE);
	enginesScrolledwindow->set_flags(CAN_FOCUS);
	enginesScrolledwindow->set_shadow_type(SHADOW_NONE);
	enginesScrolledwindow->set_policy(POLICY_AUTOMATIC, POLICY_AUTOMATIC);
	enginesScrolledwindow->property_window_placement().set_value(CORNER_TOP_LEFT);
	enginesScrolledwindow->add(*this);

	// Position the scrolled window
	enginesVbox->pack_start(*enginesScrolledwindow, Gtk::PACK_EXPAND_WIDGET, 0);

	// Associate the columns model to the engines tree
	m_refStore = TreeStore::create(m_enginesColumns);
	set_model(m_refStore);

	TreeViewColumn *pColumn = new TreeViewColumn(_("Search Engines"));
	// Pack an icon renderer for engines icons
	CellRendererPixbuf *pIconRenderer = new CellRendererPixbuf();
	pColumn->pack_start(*manage(pIconRenderer), false);
	pColumn->set_cell_data_func(*pIconRenderer, SigC::slot(*this, &EnginesTree::renderEngineIcon));
	pColumn->pack_end(m_enginesColumns.m_name, false);
	pColumn->set_sizing(TREE_VIEW_COLUMN_AUTOSIZE);
	append_column(*manage(pColumn));

	// Handle button presses
	signal_button_press_event().connect_notify(SigC::slot(*this, &EnginesTree::onButtonPressEvent));
	// Control which rows can be selected
	get_selection()->set_select_function(SigC::slot(*this, &EnginesTree::onSelectionSelect));
	// Listen for style changes
	signal_style_changed().connect_notify(SigC::slot(*this, &EnginesTree::onStyleChanged));

	// Render the icons
	m_engineFolderIconPixbuf = render_icon(Stock::DIRECTORY, ICON_SIZE_MENU, "MetaSE-pinot");

	// Populate
	populate();

	// Show all
	show();
	enginesScrolledwindow->show();
}

EnginesTree::~EnginesTree()
{
}

void EnginesTree::save(void)
{
	std::map<string, bool> &channels = m_settings.getSearchEnginesChannels();

	TreeModel::Children children = m_refStore->children();
	for (TreeModel::Children::iterator iter = children.begin(); iter != children.end(); ++iter)
	{
		TreeModel::Row row = *iter;

		if (row[m_enginesColumns.m_type] == EnginesModelColumns::ENGINE_FOLDER)
		{
			ustring channelName(row[m_enginesColumns.m_name]);
			TreeModel::Path path = m_refStore->get_path(iter);

			std::map<string, bool>::iterator channelIter = channels.find(from_utf8(channelName));
			if (channelIter != channels.end())
			{
#ifdef DEBUG
				cout << "EnginesTree::save: " << channelName << " is " << row_expanded(path) << endl;
#endif
				channelIter->second = row_expanded(path);
			}
		}
	}
}

void EnginesTree::renderEngineIcon(Gtk::CellRenderer *renderer, const Gtk::TreeModel::iterator &iter)
{
	TreeModel::Row row = *iter;

	if (renderer == NULL)
	{
		return;
	}

	CellRendererPixbuf *pIconRenderer = dynamic_cast<CellRendererPixbuf*>(renderer);
	if (pIconRenderer != NULL)
	{
		// Is this an engine folder ?
		if (row[m_enginesColumns.m_type] == EnginesModelColumns::ENGINE_FOLDER)
		{
			pIconRenderer->property_pixbuf() = m_engineFolderIconPixbuf;
		}
		else
		{
			pIconRenderer->property_pixbuf().reset_value();
		}
	}
}

//
// Handles button presses.
//
void EnginesTree::onButtonPressEvent(GdkEventButton *ev)
{
	list<TreeModel::Path> selectedEngines = get_selection()->get_selected_rows();
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

	TreeModel::iterator engineIter = m_refStore->get_iter(*enginePath);
	TreeModel::Row engineRow = *engineIter;

	// Check for double clicks
	if (ev->type == GDK_2BUTTON_PRESS)
	{
#ifdef DEBUG
		cout << "EnginesTree::onButtonPressEvent: double click on button " << ev->button << endl;
#endif
		// Make sure the engine is an external index
		EnginesModelColumns::EngineType engineType = engineRow[m_enginesColumns.m_type];
		if (engineType == EnginesModelColumns::INDEX_ENGINE)
		{
			ustring name = engineRow[m_enginesColumns.m_name];
			ustring location = engineRow[m_enginesColumns.m_option];

			m_signalDoubleClick(name, location);
		}
		else
		{
			// Is the row already expanded ?
			if (row_expanded(*enginePath) == false)
			{
				// Expand it
				expand_row(*enginePath, true);
			}
			else
			{
				// Collapse it
				collapse_row(*enginePath);
			}
		}
	}
}

//
// Handles attempts to select rows.
//
bool EnginesTree::onSelectionSelect(const RefPtr<TreeModel>& model,
		const TreeModel::Path& path, bool path_currently_selected)
{
	// All nodes can be selected
	return true;
}

//
// Handles GTK style changes.
//
void EnginesTree::onStyleChanged(const RefPtr<Style> &previous_style)
{
#ifdef DEBUG
	cout << "EnginesTree::onStyleChanged: called" << endl;
#endif
	// FIXME: find better icons :-)
	m_engineFolderIconPixbuf = render_icon(Stock::DIRECTORY, ICON_SIZE_MENU, "MetaSE-pinot");
}

//
// Gets a list of selected items.
//
list<TreeModel::Path> EnginesTree::getSelection(void)
{
	return get_selection()->get_selected_rows();
}

/// Gets an iterator.
TreeModel::iterator EnginesTree::getIter(TreeModel::Path enginePath)
{
	return m_refStore->get_iter(enginePath);
}

/// Gets the column record.
EnginesModelColumns &EnginesTree::getColumnRecord(void)
{
	return m_enginesColumns;
}

//
// Populate the tree.
//
void EnginesTree::populate(bool indexesOnly)
{
	set<PinotSettings::Engine> engines;
	TreeModel::Row row;

	// Reset the whole tree
	get_selection()->unselect_all();
	m_refStore->clear();

	// Populate the tree with search engines
	std::map<string, bool> &channels = m_settings.getSearchEnginesChannels();
	for (std::map<string, bool>::const_iterator channelIter = channels.begin();
		channelIter != channels.end(); ++channelIter)
	{
		string channelName(channelIter->first);
		bool isExpanded(channelIter->second);

		// Enumerate search engines for this channel
		engines.clear();
		m_settings.getSearchEngines(engines, channelName);

		if (engines.empty() == true)
		{
			continue;
		}

		TreeModel::iterator folderIter = m_refStore->append();
		row = *folderIter;

		row[m_enginesColumns.m_name] = to_utf8(channelName);
		row[m_enginesColumns.m_engineName] = "internal-folder";
		row[m_enginesColumns.m_option] = "";
		row[m_enginesColumns.m_type] = EnginesModelColumns::ENGINE_FOLDER;

		std::set<PinotSettings::Engine>::const_iterator engineIter = engines.begin();
		for (; engineIter != engines.end(); ++engineIter)
		{
			string engineName = engineIter->m_name;
			string engineType = engineIter->m_type;

			if (engineType == "xapian")
			{
				// Skip Xapian, it's only usable through a local engine
				continue;
			}

			TreeModel::iterator iter = m_refStore->append(folderIter->children());
			row = *iter;

			row[m_enginesColumns.m_name] = to_utf8(engineName);
			row[m_enginesColumns.m_engineName] = to_utf8(engineType);
			// Is this a Sherlock engine ?
			if (engineType == "sherlock")
			{
				row[m_enginesColumns.m_option] = engineIter->m_option;
			}
			else
			{
				row[m_enginesColumns.m_option] = "";
			}
			row[m_enginesColumns.m_type] = EnginesModelColumns::WEB_ENGINE;
		}

		TreeModel::Path folderPath = m_refStore->get_path(folderIter);
		if (isExpanded == true)
		{
			// Expand this
			expand_row(folderPath, true);
		}
		else
		{
			// Collapse this
			collapse_row(folderPath);
		}
	}

	// Local engines
	TreeModel::iterator localIter = m_refStore->append();
	row = *localIter;
	row[m_enginesColumns.m_name] = _("Current User");
	row[m_enginesColumns.m_engineName] = "internal-folder";
	row[m_enginesColumns.m_option] = "";
	row[m_enginesColumns.m_type] = EnginesModelColumns::ENGINE_FOLDER;

	std::map<std::string, std::string>::const_iterator indexIter = m_settings.getIndexes().begin();
	for (; indexIter != m_settings.getIndexes().end(); ++indexIter)
	{
		ustring indexName = to_utf8(indexIter->first);
		EnginesModelColumns::EngineType indexType = EnginesModelColumns::INDEX_ENGINE;

		if ((indexName == _("My Web Pages")) ||
			(indexName == _("My Documents")))
		{
			indexType = EnginesModelColumns::INTERNAL_INDEX_ENGINE;
		}

		TreeModel::iterator iter = m_refStore->append(localIter->children());
		TreeModel::Row row = *iter;
		row[m_enginesColumns.m_name] = indexName;
		row[m_enginesColumns.m_engineName] = "xapian";
		row[m_enginesColumns.m_option] = to_utf8(indexIter->second);
		row[m_enginesColumns.m_type] = indexType;
	}

	// Expand this
	expand_row(m_refStore->get_path(localIter), true);

	get_selection()->select(localIter);
}

//
// Clear the tree.
//
void EnginesTree::clear(void)
{
	// Unselect engines
	get_selection()->unselect_all();

	// Remove existing rows in the tree
	TreeModel::Children children = m_refStore->children();
	if (children.empty() == false)
	{
		TreeModel::Children::iterator iter = children.begin();
		while (iter != children.end())
		{
			// Erase this row
			m_refStore->erase(*iter);

			// Get the new first row
			children = m_refStore->children();
			iter = children.begin();
		}
		m_refStore->clear();
	}
}

//
// Returns the double-click signal.
//
Signal2<void, string, string>& EnginesTree::getDoubleClickSignal(void)
{
	return m_signalDoubleClick;
}
