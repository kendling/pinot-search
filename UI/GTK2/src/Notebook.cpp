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

#if _USE_BUTTON_TAB
#include <gtkmm/rc.h>
#endif

#include "config.h"
#include "NLS.h"
#include "Notebook.h"
#include "PinotUtils.h"

using namespace Glib;
using namespace Gtk;

NotebookPageBox::NotebookPageBox(const ustring &title, NotebookPageBox::PageType type,
	PinotSettings &settings) :
	VBox(),
	m_title(title),
	m_type(type),
	m_settings(settings)
{
}

NotebookPageBox::~NotebookPageBox()
{
}

//
// Returns the page title.
//
ustring NotebookPageBox::getTitle(void) const
{
	return m_title;
}

//
// Returns the page type.
//
NotebookPageBox::PageType NotebookPageBox::getType(void) const
{
	return m_type;
}

ResultsPage::ResultsPage(const ustring &queryName, ResultsTree *pTree,
	int parentHeight, PinotSettings &settings) :
	NotebookPageBox(queryName, NotebookPageBox::RESULTS_PAGE, settings),
	m_pVPaned(NULL),
	m_pTree(pTree)
{
	if (pTree != NULL)
	{
		m_pVPaned = manage(new VPaned());
		m_pVPaned->set_flags(Gtk::CAN_FOCUS);
		m_pVPaned->set_position(105);
		m_pVPaned->pack1(*pTree->getResultsScrolledWindow(), Gtk::EXPAND|Gtk::SHRINK);
		m_pVPaned->pack2(*pTree->getExtractScrolledWindow(), Gtk::SHRINK);
		pack_start(*m_pVPaned, Gtk::PACK_EXPAND_WIDGET, 0);

		// Give the extract 2/10th of the height
		m_pVPaned->set_position((parentHeight * 8) / 10);
		m_pVPaned->show();
	}

	show();
}

ResultsPage::~ResultsPage()
{
}

//
// Returns the page's tree.
//
ResultsTree *ResultsPage::getTree(void) const
{
	return m_pTree;
}

bool NotebookTabBox::m_initialized = false;

NotebookTabBox::NotebookTabBox(const Glib::ustring &title, NotebookPageBox::PageType type) :
	HBox(),
	m_title(title),
	m_pageType(type),
	m_tabLabel(NULL),
	m_tabImage(NULL),
#if _USE_BUTTON_TAB
	m_tabButton(NULL)
#else
	m_tabEventBox(NULL)
#endif
{
	if (m_initialized == false)
	{
		m_initialized = true;

#if _USE_BUTTON_TAB
		// This was lifted from gnome-terminal's terminal-window.c
		RC::parse_string("style \"pinot-tab-close-button-style\"\n"
			"{\n"
			"GtkWidget::focus-padding = 0\n"
			"GtkWidget::focus-line-width = 0\n"
			"xthickness = 0\n"
			"ythickness = 0\n"
			"}\n"
			"widget \"*.pinot-tab-close-button\" style \"pinot-tab-close-button-style\"");
#endif
	}

	m_tabLabel = manage(new Label(title));
	m_tabImage = manage(new Image(StockID("gtk-close"), IconSize(ICON_SIZE_MENU)));
#if _USE_BUTTON_TAB
	m_tabButton = manage(new Button());
#else
	m_tabEventBox = manage(new EventBox);
#endif

	m_tabLabel->set_alignment(0, 0.5);
	m_tabLabel->set_padding(0, 0);
	m_tabLabel->set_justify(JUSTIFY_LEFT);
	m_tabLabel->set_line_wrap(false);
	m_tabLabel->set_use_markup(false);
	m_tabLabel->set_selectable(false);
	m_tabImage->set_alignment(0, 0);
	m_tabImage->set_padding(0, 0);
#if _USE_BUTTON_TAB
	m_tabButton->set_relief(RELIEF_NONE);
	m_tabButton->set_border_width(0);
	m_tabButton->set_name("pinot-tab-close-button");
	m_tabButton->set_tooltip_text(_("Close"));
	m_tabButton->set_alignment(0, 0);
	m_tabButton->add(*m_tabImage);
#else
	m_tabEventBox->add(*m_tabImage);
	m_tabEventBox->set_events(Gdk::BUTTON_PRESS_MASK);
#endif
	pack_start(*m_tabLabel);
#if _USE_BUTTON_TAB
	pack_start(*m_tabButton, PACK_SHRINK);
#else
	pack_start(*m_tabEventBox, PACK_SHRINK);
#endif
	set_spacing(0);
	set_homogeneous(false);
	m_tabLabel->show();
	m_tabImage->show();
#if _USE_BUTTON_TAB
	m_tabButton->show();
#else
	m_tabEventBox->show();
#endif
	show();

#if _USE_BUTTON_TAB
	m_tabButton->signal_clicked().connect(
		sigc::mem_fun(*this, &NotebookTabBox::onButtonClicked));
#else
	m_tabEventBox->signal_button_press_event().connect(
		sigc::mem_fun(*this, &NotebookTabBox::onButtonPressEvent));
#endif
}

NotebookTabBox::~NotebookTabBox()
{
}

#if _USE_BUTTON_TAB
void NotebookTabBox::onButtonClicked(void)
#else
bool NotebookTabBox::onButtonPressEvent(GdkEventButton *ev)
#endif
{
	m_signalClose(m_title, m_pageType);

#if !_USE_BUTTON_TAB
	return true;
#endif
}

//
// Returns the close signal.
//
sigc::signal2<void, ustring, NotebookPageBox::PageType>& NotebookTabBox::getCloseSignal(void)
{
	return m_signalClose;
}
