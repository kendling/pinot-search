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

#include <ctype.h>
#include <iostream>
#include <gtkmm/alignment.h>
#include <gtkmm/box.h>
#include <gtkmm/buttonbox.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/stock.h>
#include <gtkmm/textbuffer.h>

#include "StringManip.h"
#include "Url.h"
#include "QueryHistory.h"
#include "ViewHistory.h"
#include "XapianIndex.h"
#include "config.h"
#include "NLS.h"
#include "PinotSettings.h"
#include "PinotUtils.h"
#include "ResultsTree.h"

using namespace std;
using namespace SigC;
using namespace Glib;
using namespace Gdk;
using namespace Gtk;

ResultsTree::ResultsTree(const ustring &queryName, Menu *pPopupMenu,
	PinotSettings &settings) :
	TreeView(),
	m_queryName(queryName),
	m_pPopupMenu(pPopupMenu),
	m_pResultsScrolledwindow(NULL),
	m_settings(settings),
	m_pExtractScrolledwindow(NULL),
	m_extractTextview(NULL),
	m_showExtract(true),
	m_groupBySearchEngine(true)
{
	m_pResultsScrolledwindow = manage(new ScrolledWindow());
	m_pExtractScrolledwindow = manage(new ScrolledWindow());
	m_extractTextview = manage(new TextView());

	// This is the actual results tree
	set_events(Gdk::BUTTON_PRESS_MASK);
	set_flags(CAN_FOCUS);
	set_headers_visible(true);
	set_rules_hint(true);
	set_reorderable(false);
	set_enable_search(true);
	m_pResultsScrolledwindow->set_flags(CAN_FOCUS);
	m_pResultsScrolledwindow->set_border_width(4);
	m_pResultsScrolledwindow->set_shadow_type(SHADOW_NONE);
	m_pResultsScrolledwindow->set_policy(POLICY_AUTOMATIC, POLICY_AUTOMATIC);
	m_pResultsScrolledwindow->property_window_placement().set_value(CORNER_TOP_LEFT);
	m_pResultsScrolledwindow->add(*this);

	// That's for the extract view
	m_extractTextview->set_flags(CAN_FOCUS);
	m_extractTextview->set_editable(false);
	m_extractTextview->set_cursor_visible(false);
	m_extractTextview->set_pixels_above_lines(0);
	m_extractTextview->set_pixels_below_lines(0);
	m_extractTextview->set_pixels_inside_wrap(0);
	m_extractTextview->set_left_margin(0);
	m_extractTextview->set_right_margin(0);
	m_extractTextview->set_indent(0);
	m_extractTextview->set_wrap_mode(WRAP_WORD);
	m_extractTextview->set_justification(JUSTIFY_LEFT);
	m_pExtractScrolledwindow->set_flags(CAN_FOCUS);
	m_pExtractScrolledwindow->set_border_width(4);
	m_pExtractScrolledwindow->set_shadow_type(SHADOW_NONE);
	m_pExtractScrolledwindow->set_policy(POLICY_AUTOMATIC, POLICY_AUTOMATIC);
	m_pExtractScrolledwindow->property_window_placement().set_value(CORNER_TOP_LEFT);
	m_pExtractScrolledwindow->add(*m_extractTextview);

	// Create a blod text tag to highlight extract terms with
	RefPtr<TextBuffer> refBuffer = m_extractTextview->get_buffer();
	RefPtr<TextBuffer::Tag> refBoldTag = refBuffer->create_tag("bold-text");
	refBoldTag->property_weight() = Pango::WEIGHT_BOLD;

	// Associate the columns model to the results tree
	m_refStore = TreeStore::create(m_resultsColumns);
	set_model(m_refStore);

	// The first column is for status icons
	TreeViewColumn *treeColumn = new TreeViewColumn("");
	// Pack an icon renderer for the viewed status
	CellRendererPixbuf *iconRenderer = new CellRendererPixbuf();
	treeColumn->pack_start(*manage(iconRenderer), false);
	treeColumn->set_cell_data_func(*iconRenderer, SigC::slot(*this, &ResultsTree::renderViewStatus));
	// Pack a second icon renderer for the indexed status
	iconRenderer = new CellRendererPixbuf();
	treeColumn->pack_start(*manage(iconRenderer), false);
	treeColumn->set_cell_data_func(*iconRenderer, SigC::slot(*this, &ResultsTree::renderIndexStatus));
	// And a third one for the ranking
	iconRenderer = new CellRendererPixbuf();
	treeColumn->pack_start(*manage(iconRenderer), false);
	treeColumn->set_cell_data_func(*iconRenderer, SigC::slot(*this, &ResultsTree::renderRanking));
	treeColumn->set_resizable(true);
	append_column(*manage(treeColumn));

	// This is the title column
	treeColumn = new TreeViewColumn(_("Title"));
	CellRendererText *textCellRenderer = new CellRendererText();
	treeColumn->pack_start(*manage(textCellRenderer));
	treeColumn->set_cell_data_func(*textCellRenderer, SigC::slot(*this, &ResultsTree::renderBackgroundColour));
	treeColumn->add_attribute(textCellRenderer->property_text(), m_resultsColumns.m_text);
	treeColumn->set_resizable(true);
	append_column(*manage(treeColumn));

	// The last column is for the URL
	append_column(_("URL"), m_resultsColumns.m_url);

	// Make headers clickable
	set_headers_clickable(true);
	// Allow multiple selection
	get_selection()->set_mode(SELECTION_MULTIPLE);

	// Connect the signals
	signal_button_press_event().connect_notify(
		SigC::slot(*this, &ResultsTree::onButtonPressEvent));
	get_selection()->signal_changed().connect(
		SigC::slot(*this, &ResultsTree::onSelectionChanged));

	// Enable interactive search
	set_search_column(m_resultsColumns.m_text.index());
	// Control which rows can be selected
	get_selection()->set_select_function(SigC::slot(*this, &ResultsTree::onSelectionSelect));
	// Listen for style changes
	signal_style_changed().connect_notify(SigC::slot(*this, &ResultsTree::onStyleChanged));

	// Render the icons
	m_indexedIconPixbuf = render_icon(Stock::INDEX, ICON_SIZE_MENU, "MetaSE-pinot");
	m_viewededIconPixbuf = render_icon(Stock::YES, ICON_SIZE_MENU, "MetaSE-pinot");
	m_upIconPixbuf = render_icon(Stock::GO_UP, ICON_SIZE_MENU, "MetaSE-pinot");
	m_downIconPixbuf = render_icon(Stock::GO_DOWN, ICON_SIZE_MENU, "MetaSE-pinot");

	// Show all
	show();
	m_pResultsScrolledwindow->show();
	m_extractTextview->show();
	m_pExtractScrolledwindow->show();
}

ResultsTree::~ResultsTree()
{
}

void ResultsTree::renderViewStatus(CellRenderer *renderer, const TreeModel::iterator &iter)
{
	TreeModel::Row row = *iter;

	if (renderer == NULL)
	{
		return;
	}

	CellRendererPixbuf *iconRenderer = dynamic_cast<CellRendererPixbuf*>(renderer);
	if (iconRenderer != NULL)
	{
		// Has this result been already viewed ?
		if ((row[m_resultsColumns.m_viewed] == true) &&
			(m_viewededIconPixbuf))
		{
			iconRenderer->property_pixbuf() = m_viewededIconPixbuf;
		}
		else
		{
			iconRenderer->property_pixbuf().reset_value();
		}
	}
}

void ResultsTree::renderIndexStatus(CellRenderer *renderer, const TreeModel::iterator &iter)
{
	TreeModel::Row row = *iter;

	if (renderer == NULL)
	{
		return;
	}

	CellRendererPixbuf *iconRenderer = dynamic_cast<CellRendererPixbuf*>(renderer);
	if (iconRenderer != NULL)
	{
		// Is this result indexed ?
		if ((row[m_resultsColumns.m_indexed] == true) &&
			(m_indexedIconPixbuf))
		{
			iconRenderer->property_pixbuf() = m_indexedIconPixbuf;
		}
		else
		{
			iconRenderer->property_pixbuf().reset_value();
		}
	}
}

void ResultsTree::renderRanking(CellRenderer *renderer, const TreeModel::iterator &iter)
{
	TreeModel::Row row = *iter;

	if (renderer == NULL)
	{
		return;
	}

	CellRendererPixbuf *iconRenderer = dynamic_cast<CellRendererPixbuf*>(renderer);
	if (iconRenderer != NULL)
	{
		int rankDiff = row[m_resultsColumns.m_rankDiff];

		// Has this result's score changed ?
		if ((rankDiff > 0) &&
			(rankDiff != 666))
		{
			iconRenderer->property_pixbuf() = m_upIconPixbuf;
		}
		else if (rankDiff < 0)
		{
			iconRenderer->property_pixbuf() = m_downIconPixbuf;
		}
		else
		{
			iconRenderer->property_pixbuf().reset_value();
		}
	}
}

void ResultsTree::renderBackgroundColour(CellRenderer *renderer, const TreeModel::iterator &iter)
{
	TreeModel::Row row = *iter;

	if (renderer == NULL)
	{
		return;
	}

	CellRendererText *textRenderer = dynamic_cast<CellRendererText*>(renderer);
	if (textRenderer != NULL)
	{
		// Is this result new ?
		if (row[m_resultsColumns.m_rankDiff] == 666)
		{
			// Change the row's background
			textRenderer->property_background_gdk() = m_settings.m_newResultsColour;
		}
		else
		{
			textRenderer->property_background_gdk().reset_value();
		}
	}
}

void ResultsTree::onButtonPressEvent(GdkEventButton *ev)
{
	// Check for popup click
	if ((ev->type == GDK_BUTTON_PRESS) &&
		(ev->button == 3) )
	{
		if (m_pPopupMenu != NULL)
		{
			m_pPopupMenu->popup(ev->button, ev->time);
		}
	}
	// Check for double clicks
	else if (ev->type == GDK_2BUTTON_PRESS)
	{
#ifdef DEBUG
		cout << "ResultsTree::onButtonPressEvent: double click on button " << ev->button << endl;
#endif
		// Get the selected result, if any
		TreeModel::iterator iter = get_selection()->get_selected();
		if (iter)
		{
			TreeModel::Path resultPath = m_refStore->get_path(iter);
			// Is the row already expanded ?
			if (row_expanded(resultPath) == false)
			{
				// Expand it
				expand_row(resultPath, true);
			}
			else
			{
				// Collapse it
				collapse_row(resultPath);
			}
		}
	}
}

void ResultsTree::onSelectionChanged(void)
{
	m_signalSelectionChanged(m_queryName);
}

bool ResultsTree::onSelectionSelect(const RefPtr<TreeModel>& model,
		const TreeModel::Path& path, bool path_currently_selected)
{
	const TreeModel::iterator iter = model->get_iter(path);
	const TreeModel::Row row = *iter;

	m_indexNames.clear();

	if (path_currently_selected == true)
	{
#ifdef DEBUG
		cout << "ResultsTree::onSelectionSelect: unselected entry " << row[m_resultsColumns.m_text] << endl;
#endif
	}
	else
	{
#ifdef DEBUG
		cout << "ResultsTree::onSelectionSelect: selected entry " << row[m_resultsColumns.m_text] << endl;
#endif

		// Is this an actual result ?
		ResultsModelColumns::ResultType type = row[m_resultsColumns.m_type];
		if (type == ResultsModelColumns::RESULT_TITLE)
		{
			QueryHistory history(m_settings.m_historyDatabase);
			set<string> engineNames, indexNames;
			string extract;

			// The query name and row[m_resultsColumns.m_queryName] should be equal
			string url = from_utf8(row[m_resultsColumns.m_url]);
			unsigned int engineIds = row[m_resultsColumns.m_engines];
			unsigned int indexIds = row[m_resultsColumns.m_indexes];

#ifdef DEBUG
			cout << "ResultsTree::onSelectionSelect: selected result (" << engineIds << "," << indexIds << ")" << endl;
#endif
			m_settings.getEngineNames(engineIds, engineNames);
			if (engineNames.empty() == false)
			{
				// Get the first engine this result was obtained from
				string engineName = (*engineNames.begin());
				if (engineName == "Xapian")
				{
					m_settings.getIndexNames(indexIds, indexNames);
					if (indexNames.empty() == false)
					{
						// Use the name of the first index as engine name
						engineName = (*indexNames.begin());

						// Any internal index in there ?
						for (set<string>::iterator indexIter = indexNames.begin(); indexIter != indexNames.end(); ++indexIter)
						{
							if  (m_settings.isInternalIndex(*indexIter) == true)
							{
								m_indexNames.insert(*indexIter);
							}
						}
					}
				}

#ifdef DEBUG
				cout << "ResultsTree::onSelectionSelect: first engine for " << url << " was " << engineName << endl;
#endif
				extract = history.getItemExtract(from_utf8(m_queryName), engineName, url);
			}

			RefPtr<TextBuffer> refBuffer = m_extractTextview->get_buffer();
			refBuffer->set_text(to_utf8(extract));

			// Highlight the query's terms in the extract
			ustring lowerExtract(StringManip::toLowerCase(to_utf8(extract)));
			for (set<string>::iterator termIter = m_queryTerms.begin();
				termIter != m_queryTerms.end(); ++termIter)
			{
				ustring term(to_utf8(StringManip::toLowerCase(*termIter)));

				ustring::size_type pos = lowerExtract.find(term);
				while (pos != ustring::npos)
				{
					if (((pos > 0) && (isspace(lowerExtract[pos - 1]) != 0)) ||
						((pos + termIter->length() < lowerExtract.length() - 1) &&
							(isspace(lowerExtract[pos + termIter->length()]) != 0)))
					{
						// Apply the tag
						refBuffer->apply_tag_by_name("bold-text", refBuffer->get_iter_at_offset(pos),
							refBuffer->get_iter_at_offset(pos + termIter->length()));
					}

					// Next
					pos = lowerExtract.find(term, pos + 1);
				}
			}

			// The extract is not editable
			m_extractTextview->set_editable(false);
			m_extractTextview->set_cursor_visible(false);
		}
	}

	return true;
}

void ResultsTree::onStyleChanged(const RefPtr<Style> &previous_style)
{
#ifdef DEBUG
	cout << "ResultsTree::onStyleChanged: called" << endl;
#endif
	// FIXME: find better icons :-)
	m_indexedIconPixbuf = render_icon(Stock::INDEX, ICON_SIZE_MENU, "MetaSE-pinot");
	m_viewededIconPixbuf = render_icon(Stock::YES, ICON_SIZE_MENU, "MetaSE-pinot");
	m_upIconPixbuf = render_icon(Stock::GO_UP, ICON_SIZE_MENU, "MetaSE-pinot");
	m_downIconPixbuf = render_icon(Stock::GO_DOWN, ICON_SIZE_MENU, "MetaSE-pinot");
}

//
// Returns the results scrolled window.
//
ScrolledWindow *ResultsTree::getResultsScrolledWindow(void) const
{
	return m_pResultsScrolledwindow;
}

//
// Returns the extract scrolled window.
//
ScrolledWindow *ResultsTree::getExtractScrolledWindow(void) const
{
	return m_pExtractScrolledwindow;
}

//
// Adds a set of results.
// Returns true if something was added to the tree.
//
bool ResultsTree::addResults(QueryProperties &queryProps, const string &engineName,
	const vector<Result> &resultsList, const string &charset, bool groupBySearchEngine)
{
	std::map<string, TreeModel::iterator> updatedGroups;
	string registeredEngineName(engineName);
	string queryName(queryProps.getName());
	string language(queryProps.getLanguage());
	string labelName(queryProps.getLabelName());
	unsigned int count = 0;
	ResultsModelColumns::ResultType rootType;

	// Get this query's terms
	queryProps.getTerms(m_queryTerms);

	// Unselect all
	get_selection()->unselect_all();

	// What's the grouping criteria ?
	if (groupBySearchEngine == true)
	{
		// By search engine
		rootType = ResultsModelColumns::RESULT_ROOT;
	}
	else
	{
		// By host
		rootType = ResultsModelColumns::RESULT_HOST;
	}

	// Find out what the search engine ID is
	unsigned int indexId = 0;
	unsigned int engineId = m_settings.getEngineId(registeredEngineName);
	if (engineId == 0)
	{
		// Chances are this engine is an index
		std::map<string, string>::const_iterator mapIter = m_settings.getIndexes().find(registeredEngineName);
		if (mapIter != m_settings.getIndexes().end())
		{
			// Yes, it is
			indexId = m_settings.getIndexId(registeredEngineName);
			engineId = m_settings.getEngineId("Xapian");
#ifdef DEBUG
			cout << "ResultsTree::addResults: engine is index " << registeredEngineName << endl;
#endif
		}
#ifdef DEBUG
		else cout << "ResultsTree::addResults: " << registeredEngineName << " is not an index" <<  endl;
#endif
	}
#ifdef DEBUG
	cout << "ResultsTree::addResults: ID for engine " << registeredEngineName << " is " << engineId <<  endl;
#endif

	QueryHistory history(m_settings.m_historyDatabase);
	bool isNewQuery = false;
	if (history.getLastRun(queryName, engineName).empty() == true)
	{
		isNewQuery = true;
	}

	// Look at the results list
#ifdef DEBUG
	cout << "ResultsTree::addResults: " << resultsList.size() << " results to display" << endl;
#endif
	for (vector<Result>::const_iterator resultIter = resultsList.begin();
		resultIter != resultsList.end(); ++resultIter)
	{
		ustring title(to_utf8(resultIter->getTitle(), charset));
		ustring location(to_utf8(resultIter->getLocation(), charset));
		ustring extract(to_utf8(resultIter->getExtract(), charset));
		float currentScore = resultIter->getScore();
		string score;
		int rankDiff = 0;

		// What group should the result go to ?
		string groupName;
		if (rootType == ResultsModelColumns::RESULT_HOST)
		{
			Url urlObj(location);
			groupName = urlObj.getHost();
		}
		else
		{
			groupName = registeredEngineName;
		}

		// Add the group or get its position if it's already in
		TreeModel::iterator groupIter;
		if (appendGroup(groupName, rootType, groupIter) == true)
		{
			// OK, add a row for this result within the group
			TreeModel::iterator titleIter;

			// Has the result's ranking changed ?
			float oldestScore = 0;
			float previousScore = history.hasItem(queryName, registeredEngineName,
				location, oldestScore);
			if (previousScore > 0)
			{
				// Update this result whatever the current and previous rankings were
				history.updateItem(queryName, registeredEngineName, location,
					title, extract, language, currentScore);
				rankDiff = (int)(currentScore - previousScore);
			}
			else
			{
				// No, this is a new result
				history.insertItem(queryName, registeredEngineName, location,
					resultIter->getTitle(), extract, language, currentScore);
				// New results are displayed as such only if the query has already been run on the engine
				if (isNewQuery == false)
				{
					// This is a magic value :-)
					rankDiff = 666;
				}
			}

			++count;
			if (appendResult(title, location, currentScore, language, rankDiff,
				queryName, engineId, indexId, titleIter, &(*groupIter), true) == true)
			{
#ifdef DEBUG
				cout << "ResultsTree::addResults: added row for result " << count << endl;
#endif

				// Update this map, so that we know which groups need updating
				updatedGroups[groupName] = groupIter;
			}
		}
	}

	if (count > 0)
	{
#ifdef DEBUG
		cout << "ResultsTree::addResults: " << updatedGroups.size() << " groups to update" << endl;
#endif
		// Update the groups to which we have added results
		for (std::map<string, TreeModel::iterator>::iterator mapIter = updatedGroups.begin();
			mapIter != updatedGroups.end(); mapIter++)
		{
			TreeModel::iterator groupIter = mapIter->second;
			updateGroup(groupIter);
		}

		return true;
	}
	else
	{
		if (rootType == ResultsModelColumns::RESULT_ROOT)
		{
			// If this didn't return any result, add an empty group
			TreeModel::iterator groupIter;
			appendGroup(registeredEngineName, rootType, groupIter);
			updateGroup(groupIter);

			return true;
		}
	}

	return false;
}

//
// Sets how results are grouped.
//
void ResultsTree::setGroupMode(bool groupBySearchEngine)
{
	ResultsModelColumns::ResultType currentType, newType;

	if (m_groupBySearchEngine == groupBySearchEngine)
	{
		// No change
		return;
	}
	m_groupBySearchEngine = groupBySearchEngine;

	// Do we need to update the tree ?
	TreeModel::Children children = m_refStore->children();
	if (children.empty() == true)
	{
		return;
	}

	// What's the new grouping criteria ?
	if (groupBySearchEngine == true)
	{
		// By search engine
		currentType = ResultsModelColumns::RESULT_HOST;
		newType = ResultsModelColumns::RESULT_ROOT;
	}
	else
	{
		// By host
		currentType = ResultsModelColumns::RESULT_ROOT;
		newType = ResultsModelColumns::RESULT_HOST;
	}

	// Clear the map
	m_resultsGroups.clear();

	// Unselect results
	get_selection()->unselect_all();

	TreeModel::Children::iterator iter = children.begin();
	while (iter != children.end())
	{
		TreeModel::Row row = *iter;
#ifdef DEBUG
		cout << "ResultsTree::groupBySearchEngine: looking at " << row[m_resultsColumns.m_text] << endl;
#endif
		ResultsModelColumns::ResultType type = row[m_resultsColumns.m_type];
		// Skip new type rows
		if (type == newType)
		{
			iter++;
			continue;
		}

		TreeModel::Children child = iter->children();
		if (child.empty() == false)
		{
			TreeModel::Children::iterator childIter = child.begin();
			// Type RESULT_TITLE
			while (childIter != child.end())
			{
				TreeModel::Row childRow = *childIter;
				TreeModel::iterator groupIter, newIter;
				bool success = false;

				// We will need the URL and engines columns in all cases
				string url = from_utf8(childRow[m_resultsColumns.m_url]);
				unsigned int engineIds = childRow[m_resultsColumns.m_engines];
				unsigned int indexIds = childRow[m_resultsColumns.m_indexes];

				// Get the name of the group this should go into
				if (newType == ResultsModelColumns::RESULT_HOST)
				{
					Url urlObj(url);
#ifdef DEBUG
					cout << "ResultsTree::groupBySearchEngine: row " << url << endl;
#endif
					string groupName = urlObj.getHost();
					// Add group
					if (appendGroup(groupName, newType, groupIter) == true)
					{
						// Add result
						success = appendResult(childRow[m_resultsColumns.m_text],
							childRow[m_resultsColumns.m_url],
							(float)atof(from_utf8(childRow[m_resultsColumns.m_score]).c_str()),
							from_utf8(childRow[m_resultsColumns.m_language]),
							childRow[m_resultsColumns.m_rankDiff],
							from_utf8(childRow[m_resultsColumns.m_queryName]),
							engineIds, indexIds, newIter, &(*groupIter), true);
					}
				}
				else
				{
					// Look at the engines column and see which engines this result is for
					set<string> engineNames;
					m_settings.getEngineNames(engineIds, engineNames);
					if (engineNames.empty() == false)
					{
#ifdef DEBUG
						cout << "ResultsTree::groupBySearchEngine: row is for " << engineNames.size() << endl;
#endif
						// Are there indexes in the list ?
						set<string>::iterator xapianIter = engineNames.find("Xapian");
						if ((xapianIter != engineNames.end()) &&
							(indexIds > 0))
						{
							// Erase this
							engineNames.erase(xapianIter);

							// Add entries for each index name so that we can loop once on engine names
							set<string> indexNames;
							m_settings.getIndexNames(indexIds, indexNames);
							for (set<string>::iterator iter = indexNames.begin(); iter != indexNames.end(); ++iter)
							{
								string indexName = (*iter);
								engineNames.insert(indexName);
#ifdef DEBUG
								cout << "ResultsTree::groupBySearchEngine: row is for index " << indexName << endl;
#endif
							}
						}

						for (set<string>::iterator iter = engineNames.begin(); iter != engineNames.end(); ++iter)
						{
							string engineName = (*iter);
							unsigned int indexId = 0;
							unsigned int engineId = m_settings.getEngineId(engineName);

							if (engineId == 0)
							{
								// This is actually an index, not an engine...
								indexId = m_settings.getIndexId(engineName);
								if (indexId > 0)
								{
									engineId = m_settings.getEngineId("Xapian");
								}
							}

							// Add group
							if (appendGroup(engineName, newType, groupIter) == true)
							{
								// Add result
								appendResult(childRow[m_resultsColumns.m_text],
									childRow[m_resultsColumns.m_url],
									(float)atof(from_utf8(childRow[m_resultsColumns.m_score]).c_str()),
									from_utf8(childRow[m_resultsColumns.m_language]),
									childRow[m_resultsColumns.m_rankDiff],
									from_utf8(childRow[m_resultsColumns.m_queryName]),
									engineId, indexId,
									newIter, &(*groupIter), true);
#ifdef DEBUG
								cout << "ResultsTree::groupBySearchEngine: row for " << *iter << endl;
#endif
							}
						}

						// FIXME: make sure at least one row was added
						success = true;
					}
				}

				if (success == true)
				{
					// Delete it
					m_refStore->erase(*childIter);
					childIter = child.begin();
				}
				else
				{
					// Don't delete anything then, just go to the next child
					childIter++;
				}
			}
		}

		// Erase this row
		m_refStore->erase(*iter);

		// Get the new first row, that way we don't have to worry about iterators validity
		iter = children.begin();
	}

	for (std::map<string, TreeModel::iterator>::iterator mapIter = m_resultsGroups.begin();
		mapIter != m_resultsGroups.end(); mapIter++)
	{
		TreeModel::iterator groupIter = mapIter->second;
		updateGroup(groupIter);
	}

	onSelectionChanged();
}

//
// Determines if results are selected.
//
bool ResultsTree::checkSelection(void)
{
	bool goodSel = true;

#ifdef DEBUG
	cout << "ResultsTree::checkSelection: called" << endl;
#endif
	list<TreeModel::Path> selectedItems = get_selection()->get_selected_rows();
	if (selectedItems.empty() == true)
	{
		return false;
	}

	// Go through selected items
	for (list<TreeModel::Path>::iterator itemPath = selectedItems.begin();
		itemPath != selectedItems.end(); ++itemPath)
	{
		TreeModel::iterator iter = m_refStore->get_iter(*itemPath);
		TreeModel::Row row = *iter;

		// Check only results are selected
		ResultsModelColumns::ResultType type = row[m_resultsColumns.m_type];
		if (type != ResultsModelColumns::RESULT_TITLE)
		{
			goodSel = false;
		}
	}

	return goodSel;
}

//
// Gets the first selected item.
//
Result ResultsTree::getFirstSelection(void)
{
	list<TreeModel::Path> selectedItems = get_selection()->get_selected_rows();
	if (selectedItems.empty() == true)
	{
		return Result("", "", "", "");
	}

	list<TreeModel::Path>::iterator itemPath = selectedItems.begin();
	TreeModel::iterator iter = m_refStore->get_iter(*itemPath);
	TreeModel::Row row = *iter;
	return Result(from_utf8(row[m_resultsColumns.m_url]),
		from_utf8(row[m_resultsColumns.m_text]),
		"", from_utf8(row[m_resultsColumns.m_language]));
}

//
// Gets a list of selected items.
//
bool ResultsTree::getSelection(vector<DocumentInfo> &resultsList)
{
	list<TreeModel::Path> selectedItems = get_selection()->get_selected_rows();
	if (selectedItems.empty() == true)
	{
		return false;
	}

	// Go through selected items
	for (list<TreeModel::Path>::iterator itemPath = selectedItems.begin();
		itemPath != selectedItems.end(); ++itemPath)
	{
		TreeModel::iterator iter = m_refStore->get_iter(*itemPath);
		TreeModel::Row row = *iter;

		resultsList.push_back(DocumentInfo(from_utf8(row[m_resultsColumns.m_text]),
			from_utf8(row[m_resultsColumns.m_url]),
			"", from_utf8(row[m_resultsColumns.m_language])));
	}
#ifdef DEBUG
	cout << "ResultsTree::getSelection: " << resultsList.size() << " results selected" << endl;
#endif

	return true;
}

//
// Sets the selected items' viewed state.
//
void ResultsTree::setSelectionViewedState(bool viewed)
{
	list<TreeModel::Path> selectedItems = get_selection()->get_selected_rows();
	if (selectedItems.empty() == true)
	{
		return;
	}

	// Go through selected items
	for (list<TreeModel::Path>::iterator itemPath = selectedItems.begin();
		itemPath != selectedItems.end(); ++itemPath)
	{
		TreeModel::iterator iter = m_refStore->get_iter(*itemPath);
		TreeModel::Row row = *iter;

		row[m_resultsColumns.m_viewed] = viewed;
	}
}

//
// Deletes the current selection.
//
bool ResultsTree::deleteSelection(void)
{
	bool empty = false;

	// Go through selected items
	list<TreeModel::Path> selectedItems = get_selection()->get_selected_rows();
	list<TreeModel::Path>::iterator itemPath = selectedItems.begin();
	while (itemPath != selectedItems.end())
	{
		TreeModel::iterator iter = m_refStore->get_iter(*itemPath);
		TreeModel::Row row = *iter;
		TreeModel::iterator parentIter;
		bool updateParent = false;

		// This could be a group that's in the map and should be removed first
		if (row[m_resultsColumns.m_type] != ResultsModelColumns::RESULT_TITLE)
		{
			string groupName = from_utf8(row[m_resultsColumns.m_text]);
			std::map<string, TreeModel::iterator>::iterator mapIter = m_resultsGroups.find(groupName);
			if (mapIter != m_resultsGroups.end())
			{
				m_resultsGroups.erase(mapIter);
#ifdef DEBUG
				cout << "ResultsTree::deleteResults: erased group " << groupName << endl;
#endif
			}
		}
		else
		{
			// This item is a result
			parentIter = row.parent();
			updateParent = true;
		}

		// Unselect and erase
		get_selection()->unselect(iter);
		m_refStore->erase(row);

		// Update group ?
		if (updateParent == true)
		{
			// Update the group this result belongs to
			updateGroup(parentIter);
		}

		selectedItems = get_selection()->get_selected_rows();
		itemPath = selectedItems.begin();
	}

	TreeModel::Children children = m_refStore->children();
	empty = children.empty();

	refresh();

	return empty;
}

//
// Refreshes the tree.
//
void ResultsTree::refresh(void)
{
	// FIXME: not sure why, but this helps with refreshing the tree
	columns_autosize();
}

//
// Clears the tree.
//
void ResultsTree::clear(void)
{
	// Unselect results
	get_selection()->unselect_all();

	// Remove existing rows in the tree
	TreeModel::Children children = m_refStore->children();
	if (children.empty() == false)
	{
		// Clear the groups map
		m_resultsGroups.clear();

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

		// Clear the extract field
		RefPtr<TextBuffer> refBuffer = m_extractTextview->get_buffer();
		refBuffer->set_text("");
		m_extractTextview->set_editable(false);
		m_extractTextview->set_cursor_visible(false);

		onSelectionChanged();
	}
}

//
// Shows or hides the extract field.
//
void ResultsTree::showExtract(bool show)
{
	m_showExtract = show;
	if (m_showExtract == true)
	{
		// Show the extract
		m_pExtractScrolledwindow->show();
	}
	else
	{
		// Hide
		m_pExtractScrolledwindow->hide();
	}
}

//
// Returns the changed selection signal.
//
Signal1<void, ustring>& ResultsTree::getSelectionChangedSignal(void)
{
	return m_signalSelectionChanged;
}

//
// Adds a new row in the results tree.
//
bool ResultsTree::appendResult(const ustring &text, const ustring &url,
	float score, const string &language, int rankDiff,
	const string &queryName, unsigned int engineId, unsigned int indexId,
	TreeModel::iterator &newRowIter, const TreeModel::Row *parentRow, bool noDuplicates)
{
	if (parentRow == NULL)
	{
		newRowIter = m_refStore->append();
	}
	else
	{
		// Merge duplicates within groups ?
		if (noDuplicates == true)
		{
			// Look for a row with the same URL and query. For instance, in group
			// by host mode, if a page is returned by several search engines, it
			// should appear only once
			TreeModel::Children children = parentRow->children();
			if (children.empty() == false)
			{
				TreeModel::Children::iterator childIter = children.begin();
				for (; childIter != children.end(); ++childIter)
				{
					TreeModel::Row row = *childIter;
					if ((row[m_resultsColumns.m_url] == to_utf8(url)) &&
						(row[m_resultsColumns.m_queryName] == to_utf8(queryName)))
					{
						// Update the engines column...
						row[m_resultsColumns.m_engines] = row[m_resultsColumns.m_engines] | engineId;
						// ...and the indexes column too
						row[m_resultsColumns.m_indexes] = row[m_resultsColumns.m_indexes] | engineId;
#ifdef DEBUG
						cout << "ResultsTree::appendResult: merged " << text << " " << engineId << " (" << row[m_resultsColumns.m_engines] << "," << row[m_resultsColumns.m_indexes] << ")" << endl;
#endif

						newRowIter = childIter;
						return true;
					}
				}
			}
		}

		newRowIter = m_refStore->append(parentRow->children());
#ifdef DEBUG
		cout << "ResultsTree::appendResult: added " << text << ", " << score << " to "
			<< (*parentRow)[m_resultsColumns.m_text] << endl;
#endif
	}

	XapianIndex index(m_settings.m_indexLocation);
	ViewHistory viewHistory(m_settings.m_historyDatabase);
	bool isIndexed = false;

	// Is this document indexed ?
	if ((index.isGood() == true) &&
		(index.hasDocument(url) > 0))
	{
		isIndexed = true;
	}

	// Has it been already viewed ?
	bool wasViewed = viewHistory.hasItem(url);

	char scoreStr[128];
	snprintf(scoreStr, 128, "%.f", score);

	TreeModel::Row childRow = *newRowIter;
	updateRow(childRow, text, url, scoreStr,
		to_utf8(language), to_utf8(queryName), engineId, indexId,
		ResultsModelColumns::RESULT_TITLE, isIndexed,
		wasViewed, rankDiff);

	return true;
}

//
// Adds a results group
//
bool ResultsTree::appendGroup(const string &groupName,
	ResultsModelColumns::ResultType groupType, TreeModel::iterator &groupIter)
{
	bool success = false;

	// Is this group already in ?
	std::map<string, TreeModel::iterator>::iterator mapIter = m_resultsGroups.find(groupName);
	if (mapIter == m_resultsGroups.end())
	{
		// No, it isn't: insert a new group in the tree
		groupIter = m_refStore->append();
		TreeModel::Row groupRow = *groupIter;
		updateRow(groupRow, to_utf8(groupName),
				"", "", "", "", 0, 0, groupType,
				false, false, false);

		// Update the map
		m_resultsGroups[groupName] = groupIter;
		success = true;
#ifdef DEBUG
		cout << "ResultsTree::appendGroup: updated map with " << groupName << endl;
#endif
	}
	else
	{
		// Yes, it is
		groupIter = mapIter->second;
#ifdef DEBUG
		cout << "ResultsTree::appendGroup: found " << groupName << " in map" << endl;
#endif
		success = true;
	}

	return success;
}

//
// Updates a results group.
//
void ResultsTree::updateGroup(TreeModel::iterator &groupIter)
{
	TreeModel::Row groupRow = (*groupIter);

	// Check the iterator doesn't point to a result
	if (groupRow[m_resultsColumns.m_type] == ResultsModelColumns::RESULT_TITLE)
	{
		return;
	}

	// Modify the "score" column to indicate the number of results in that group
	TreeModel::Children groupChildren = groupIter->children();
	char scoreStr[64];
	snprintf(scoreStr, 64, "%u", groupChildren.size());
	groupRow[m_resultsColumns.m_score] = scoreStr;
#ifdef DEBUG
	cout << "ResultsTree::updateGroup: group " << groupRow[m_resultsColumns.m_text] << " has " << groupChildren.size() << " children" << endl;
#endif

	// Expand this group
	TreeModel::Path groupPath = m_refStore->get_path(groupIter);
	expand_row(groupPath, true);
}

//
// Updates a row.
//
void ResultsTree::updateRow(TreeModel::Row &row, const ustring &text,
	const ustring &url, const ustring &score, const ustring &language,
	const ustring &queryName, unsigned int engineId,  unsigned int indexId,
	ResultsModelColumns::ResultType type, bool indexed, bool viewed, int rankDiff)
{
	row[m_resultsColumns.m_text] = text;
	row[m_resultsColumns.m_url] = url;
	row[m_resultsColumns.m_score] = score;
	row[m_resultsColumns.m_language] = language;
	row[m_resultsColumns.m_queryName] = queryName;
	row[m_resultsColumns.m_engines] = engineId;
	row[m_resultsColumns.m_indexes] = indexId;
	row[m_resultsColumns.m_type] = type;

	row[m_resultsColumns.m_indexed] = indexed;
	row[m_resultsColumns.m_viewed] = viewed;
	row[m_resultsColumns.m_rankDiff] = rankDiff;
}
