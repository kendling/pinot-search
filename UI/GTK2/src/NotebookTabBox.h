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

#ifndef _NOTEBOOKTABBOX_HH
#define _NOTEBOOKTABBOX_HH

#include <sigc++/signal.h>
#include <glibmm/ustring.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/image.h>
#if _USE_BUTTON_TAB
#include <gtkmm/button.h>
#else
#include <gtkmm/eventbox.h>
#endif

/**
  * Closable notebook tab.
  */
class NotebookTabBox : public Gtk::HBox
{
	public:
		typedef enum { RESULTS_PAGE = 0, INDEX_PAGE, VIEW_PAGE } PageType;

		NotebookTabBox(const Glib::ustring &title, PageType type);
		virtual ~NotebookTabBox();

		/// Returns the page type.
		PageType getPageType(void) const;

		/// Returns the close signal.
		SigC::Signal2<void, Glib::ustring, PageType>& getCloseSignal(void);

	protected:
		Glib::ustring m_title;
		PageType m_pageType;
		Gtk::Label *m_tabLabel;
		Gtk::Image *m_tabImage;
#if _USE_BUTTON_TAB
		Gtk::Button *m_tabButton;
#else
		Gtk::EventBox *m_tabEventBox;
#endif
		SigC::Signal2<void, Glib::ustring, PageType> m_signalClose;

#if _USE_BUTTON_TAB
		void onButtonClicked(void);
#else
		bool onButtonPressEvent(GdkEventButton *ev);
#endif

};

#endif // _NOTEBOOKTABBOX_HH
