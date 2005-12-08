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
 
#ifndef _PINOTUTILS_HH
#define _PINOTUTILS_HH

#include <string>
#include <vector>
#if GTKMM_MAJOR_VERSION==2 && GTKMM_MINOR_VERSION>2
#include <sigc++/compatibility.h>
#endif
#include <sigc++/signal.h>
#include <glibmm/ustring.h>
#include <gtkmm/window.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treemodel.h>

/**
  * DCTreeView subclasses TreeView and handles double clicks.
  * Handling double-clicks on tree views is not straight-forward, see :
  * http://bugzilla.gnome.org/show_bug.cgi?id=89780
  * http://www.gtkmm.org/gtkmm2/docs/tutorial/html/ch08s07.html
  */
class DCTreeView : public Gtk::TreeView
{
	public:
		DCTreeView() : Gtk::TreeView() {}
		virtual ~DCTreeView() {}

		SigC::Signal0<void> signal_double_click_event;

	protected:
		bool on_button_press_event(GdkEventButton* event)
		{
			// Check for double clicks
			if (event->type == GDK_2BUTTON_PRESS)
			{
				signal_double_click_event();
				return true;
			}

			// Not handled
			return TreeView::on_button_press_event(event);
		}

};

/// Open a FileSelector and request a file. Location can be initialized.
bool select_file_name(Gtk::Window &parentWindow, const Glib::ustring &title,
	Glib::ustring &location, bool openOrCreate = true, bool directoriesOnly = false);

/// Create a resizable text column.
Gtk::TreeViewColumn *create_resizable_column(const Glib::ustring &title,
	const Gtk::TreeModelColumnBase& modelColumn);

/// Create a resizable text column, rendered by renderTextCell.
Gtk::TreeViewColumn *create_resizable_column(const Glib::ustring &title,
	const Gtk::TreeModelColumnBase& modelColumn,
	const  Gtk::TreeViewColumn::SlotCellData &renderTextCell);

/// Create a resizable icon and text column, rendered by renderTextAndIconCell.
Gtk::TreeViewColumn *create_resizable_column_with_icon(const Glib::ustring &title,
	const Gtk::TreeModelColumnBase& modelColumn,
	const  Gtk::TreeViewColumn::SlotCellData &renderTextAndIconCell);

/// Converts to UTF-8, catches conversion errors
Glib::ustring to_utf8(std::string text, Glib::ustring fallback = Glib::ustring(""));

#endif // _PINOTUTILS_HH
