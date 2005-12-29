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
		IndexTree(const Glib::ustring &indexName, Gtk::Menu *pPopupMenu,
			PinotSettings &settings);
		virtual ~IndexTree();

		/// Returns the tree's scrolled window.
		Gtk::ScrolledWindow *getScrolledWindow(void) const;

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

		/// Refreshes the tree.
		void refresh(void);

		/// Returns true if the tree is empty.
		bool isEmpty(void);

		/// Clear the tree.
		void clear(void);

		/// Returns the document edit signal.
		SigC::Signal0<void>& getEditDocumentSignal(void);

		/// Returns the changed selection signal.
		SigC::Signal1<void, Glib::ustring>& getSelectionChangedSignal(void);

	protected:
		Glib::ustring m_indexName;
		Gtk::Menu *m_pPopupMenu;
		Gtk::ScrolledWindow *m_pIndexScrolledwindow;
		Glib::RefPtr<Gtk::ListStore> m_refStore;
		SigC::Signal0<void> m_signalEdit;
		SigC::Signal1<void, Glib::ustring> m_signalSelectionChanged;
		PinotSettings &m_settings;
		IndexModelColumns m_indexColumns;
		Gdk::Color m_currentLabelColour;
		bool m_showingLabel;
		bool m_listingIndex;

		void renderLabel(Gtk::CellRenderer *renderer, const Gtk::TreeModel::iterator &iter);

		void onButtonPressEvent(GdkEventButton *ev);

		void onSelectionChanged(void);

		bool onSearchEqual(const Glib::RefPtr<Gtk::TreeModel>& model, int column,
			const Glib::ustring& key, const Gtk::TreeModel::iterator& iter);

		bool onSelectionSelect(const Glib::RefPtr<Gtk::TreeModel>& model,
			const Gtk::TreeModel::Path& path, bool path_currently_selected);

	private:
		IndexTree(const IndexTree &other);
		IndexTree &operator=(const IndexTree &other);

};

#endif // _INDEXTREE_HH
