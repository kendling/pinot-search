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

#ifndef _NOTEBOOK_HH
#define _NOTEBOOK_HH

#include <sigc++/signal.h>
#include <glibmm/ustring.h>
#include <gtkmm/box.h>
#include <gtkmm/paned.h>
#include <gtkmm/label.h>
#include <gtkmm/image.h>
#if _USE_BUTTON_TAB
#include <gtkmm/button.h>
#else
#include <gtkmm/eventbox.h>
#endif

#include "HtmlView.h"
#include "PinotSettings.h"
#include "ResultsTree.h"

class NotebookPageBox : public Gtk::VBox
{
	public:
		typedef enum { RESULTS_PAGE = 0, INDEX_PAGE, VIEW_PAGE } PageType;

		NotebookPageBox(const Glib::ustring &title, PageType type,
			PinotSettings &settings);
		virtual ~NotebookPageBox();

		/// Returns the page title.
		Glib::ustring getTitle(void) const;

		/// Returns the page type.
		PageType getType(void) const;

	protected:
		Glib::ustring m_title;
		PageType m_type;
		PinotSettings &m_settings;

};

class ResultsPage : public NotebookPageBox
{
	public:
		ResultsPage(const Glib::ustring &queryName, ResultsTree *pTree,
			int parentHeight, PinotSettings &settings);
		virtual ~ResultsPage();

		/// Returns the page's tree.
		virtual ResultsTree *getTree(void) const;

	protected:
		Gtk::VPaned *m_pVPaned;
		ResultsTree *m_pTree;

};

class ViewPage : public NotebookPageBox
{
	public:
		ViewPage(const Glib::ustring &viewName, HtmlView *pView,
			PinotSettings &settings);
		virtual ~ViewPage();

		/// Returns the page's view.
		virtual HtmlView *getView(void) const;

	protected:
		HtmlView *m_pView;

};

class NotebookTabBox : public Gtk::HBox
{
	public:
		NotebookTabBox(const Glib::ustring &title, NotebookPageBox::PageType type);
		virtual ~NotebookTabBox();

		/// Returns the close signal.
		SigC::Signal2<void, Glib::ustring, NotebookPageBox::PageType>& getCloseSignal(void);

	protected:
		Glib::ustring m_title;
		NotebookPageBox::PageType m_pageType;
		Gtk::Label *m_tabLabel;
		Gtk::Image *m_tabImage;
#if _USE_BUTTON_TAB
		Gtk::Button *m_tabButton;
#else
		Gtk::EventBox *m_tabEventBox;
#endif
		SigC::Signal2<void, Glib::ustring, NotebookPageBox::PageType> m_signalClose;

#if _USE_BUTTON_TAB
		void onButtonClicked(void);
#else
		bool onButtonPressEvent(GdkEventButton *ev);
#endif

};

#endif // _NOTEBOOK_HH
