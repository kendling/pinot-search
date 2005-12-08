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

#ifndef _INDEXTREE_HH
#define _INDEXTREE_HH

#include <string>
#include <vector>
#include <sigc++/slot.h>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <gtkmm/box.h>
#include <gtkmm/menu.h>
#include <gdkmm/pixbuf.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textview.h>
#include <gtkmm/liststore.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treeselection.h>

#include "QueryProperties.h"
#include "IndexedDocument.h"
#include "ModelColumns.h"
#include "PinotSettings.h"

class IndexTree : public Gtk::TreeView
{
	public:
		IndexTree(Gtk::VBox *indexVbox, Gtk::Menu *pPopupMenu, PinotSettings &settings);
		virtual ~IndexTree();

		/**
		  * Handles selection changes.
		  * Returns true if a result is selected.
		  */
		bool onSelectionChanged(void);

		/// Adds a set of documents.
		bool addDocuments(const std::vector<IndexedDocument> &documentsList);

		/// Appends a new row in the index tree.
		bool appendDocument(const IndexedDocument &docInfo, bool labeled);

		/// Gets the first selected item's URL.
		Glib::ustring getFirstSelectionURL(void);

		/// Gets the first selected item's URL.
		Glib::ustring getFirstSelectionLiveURL(void);

		/// Gets a list of selected items.
		bool getSelection(std::vector<IndexedDocument> &documentsList);

		/// Sets the current label colour.
		void setCurrentLabelColour(unsigned short red, unsigned short green, unsigned short blue,
			bool showingLabel = true);

		/// Sets the documents that match the current label.
		void setLabel(const std::set<unsigned int> &documentsList);

		/// Sets a document's title.
		void setDocumentTitle(unsigned int docId, const std::string &text);

		/// Marks a document as labeled.
		void setDocumentLabeledState(unsigned int docId, bool labeled);

		/**
		  * Deletes the current selection.
		  * Returns true if the tree is then empty.
		  */
		bool deleteSelection(void);

		/// Returns the number of rows.
		unsigned int getRowsCount(void);

		/// Returns true if the tree is empty.
		bool isEmpty(void);

		/// Clear the tree.
		void clear(void);

		/// Returns the document edit signal.
		SigC::Signal0<void>& getEditDocumentSignal(void);

	protected:
		Glib::RefPtr<Gtk::ListStore> m_refStore;
		Gtk::Menu *m_pPopupMenu;
		SigC::Signal0<void> m_signalEdit;
		PinotSettings &m_settings;
		IndexModelColumns m_indexColumns;
		Gdk::Color m_currentLabelColour;
		bool m_showingLabel;
		bool m_listingIndex;

		void renderLabel(Gtk::CellRenderer *renderer, const Gtk::TreeModel::iterator &iter);

		/// Interactive search equal function.
		bool onSearchEqual(const Glib::RefPtr<Gtk::TreeModel>& model, int column,
			const Glib::ustring& key, const Gtk::TreeModel::iterator& iter);

		/// Handles button presses.
		void onButtonPressEvent(GdkEventButton *ev);

		/// Handles attempts to select rows.
		bool onSelectionSelect(const Glib::RefPtr<Gtk::TreeModel>& model,
			const Gtk::TreeModel::Path& path, bool path_currently_selected);

	private:
		IndexTree(const IndexTree &other);
		IndexTree &operator=(const IndexTree &other);

};

#endif // _INDEXTREE_HH
