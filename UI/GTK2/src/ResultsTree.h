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

#ifndef _RESULTSTREE_HH
#define _RESULTSTREE_HH

#include <string>
#include <vector>
#include <set>
#include <map>
#include <sigc++/slot.h>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <gdkmm/pixbuf.h>
#include <gtkmm/button.h>
#include <gtkmm/menu.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treestore.h>
#include <gtkmm/liststore.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treeselection.h>

#include "QueryProperties.h"
#include "Result.h"
#include "ModelColumns.h"
#include "PinotSettings.h"

class ResultsTree : public Gtk::TreeView
{
	public:
		ResultsTree(const Glib::ustring &queryName, Gtk::Menu *pPopupMenu,
			PinotSettings &settings);
		virtual ~ResultsTree();

		/// Returns the results scrolled window.
		Gtk::ScrolledWindow *getResultsScrolledWindow(void) const;

		/// Returns the extract scrolled window.
		Gtk::ScrolledWindow *getExtractScrolledWindow(void) const;

		/// Returns the extract tree.
		Gtk::TreeView *getExtractTree(void) const;

		/// Returns the extract.
		Glib::ustring getExtract(void) const;

		/**
		  * Adds a set of results.
		  * Returns true if something was added to the tree.
		  */
		bool addResults(QueryProperties &queryProps, const std::string &engineName,
			const std::vector<Result> &resultsList, const std::string &charset,
			bool groupBySearchEngine);

		/// Sets how results are grouped.
		void setGroupMode(bool groupBySearchEngine);

		/// Gets a list of selected items.
		bool getSelection(std::vector<DocumentInfo> &resultsList);

		/// Sets the selected items' state.
		void setSelectionState(bool viewed, bool indexed);

		/**
		  * Deletes the current selection.
		  * Returns true if the tree is then empty.
		  */
		bool deleteSelection(void);

		/// Deletes results.
		bool deleteResults(QueryProperties &queryProps, const std::string &engineName);

		/// Refreshes the tree.
		void refresh(void);

		/// Clears the tree.
		void clear(void);

		/// Shows or hides the extract field.
		void showExtract(bool show = true);

		/// Returns the changed selection signal.
		SigC::Signal1<void, Glib::ustring>& getSelectionChangedSignal(void);

		/// Returns the view results signal.
		SigC::Signal0<void>& getViewResultsSignal(void);

	protected:
		Glib::ustring m_queryName;
		Gtk::Menu *m_pPopupMenu;
		Gtk::ScrolledWindow *m_pResultsScrolledwindow;
		Glib::RefPtr<Gtk::TreeStore> m_refStore;
		SigC::Signal1<void, Glib::ustring> m_signalSelectionChanged;
		SigC::Signal0<void> m_signalViewResults;
		PinotSettings &m_settings;
		Glib::RefPtr<Gdk::Pixbuf> m_indexedIconPixbuf;
		Glib::RefPtr<Gdk::Pixbuf> m_viewededIconPixbuf;
		Glib::RefPtr<Gdk::Pixbuf> m_upIconPixbuf;
		Glib::RefPtr<Gdk::Pixbuf> m_downIconPixbuf;
		std::map<std::string, Gtk::TreeModel::iterator> m_resultsGroups;
		ResultsModelColumns m_resultsColumns;
		Gtk::ScrolledWindow *m_pExtractScrolledwindow;
		Gtk::TreeView *m_extractTreeView;
		Glib::RefPtr<Gtk::ListStore> m_refExtractStore;
		ComboModelColumns m_extractColumns;
		std::set<std::string> m_indexNames;
		bool m_showExtract;
		bool m_groupBySearchEngine;

		void renderViewStatus(Gtk::CellRenderer *pRenderer, const Gtk::TreeModel::iterator &iter);

		void renderIndexStatus(Gtk::CellRenderer *pRenderer, const Gtk::TreeModel::iterator &iter);

		void renderRanking(Gtk::CellRenderer *pRenderer, const Gtk::TreeModel::iterator &iter);

		void renderTitleColumn(Gtk::CellRenderer *pRenderer, const Gtk::TreeModel::iterator &iter);

		void renderExtractColumn(Gtk::CellRenderer *pRenderer, const Gtk::TreeModel::iterator &iter);

		void onButtonPressEvent(GdkEventButton *ev);

		void onSelectionChanged(void);

		bool onSelectionSelect(const Glib::RefPtr<Gtk::TreeModel>& model,
			const Gtk::TreeModel::Path& path, bool path_currently_selected);

		/// Handles GTK style changes.
		void onStyleChanged(const Glib::RefPtr<Gtk::Style> &previous_style);

		/// Adds a results group.
		bool appendGroup(const std::string &groupName, ResultsModelColumns::ResultType groupType,
			Gtk::TreeModel::iterator &groupIter);

		/// Adds a new row in the results tree.
		bool appendResult(const Glib::ustring &text, const Glib::ustring &url,
			int score, int rankDiff, unsigned int engineId, unsigned int indexId,
			Gtk::TreeModel::iterator &newRowIter,
			const Gtk::TreeModel::Row *parentRow = NULL, bool noDuplicates = false);

		/// Updates a results group.
		void updateGroup(Gtk::TreeModel::iterator &groupIter);

		/// Updates a row.
		void updateRow(Gtk::TreeModel::Row &row, const Glib::ustring &text,
			const Glib::ustring &url, int score, unsigned int engineId, unsigned int indexId,
			ResultsModelColumns::ResultType type, bool indexed, bool viewed, int rankDiff);

		/// Retrieves the extract to show for the given row.
		Glib::ustring findResultsExtract(const Gtk::TreeModel::Row &row);

	private:
		ResultsTree(const ResultsTree &other);
		ResultsTree &operator=(const ResultsTree &other);

};

#endif // _RESULTSTREE_HH
