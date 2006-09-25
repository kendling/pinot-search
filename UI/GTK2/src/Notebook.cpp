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

#include <sigc++/slot.h>

#include "Notebook.h"
#include "PinotUtils.h"

using namespace SigC;
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

NotebookTabBox::NotebookTabBox(const Glib::ustring &title, NotebookPageBox::PageType type) :
	HBox(),
	m_title(title),
	m_pageType(type),
	m_tabLabel(NULL),
	m_tabImage(NULL),
#if _USE_BUTTON_TAB
	m_tabImageBox(NULL),
	m_tabButton(NULL)
#else
	m_tabEventBox(NULL)
#endif
{
	int width, height;

	// Lookup the standard icon size
	bool gotDimensions = IconSize::lookup(ICON_SIZE_MENU, width, height);

	m_tabLabel = manage(new Label(title));
	m_tabImage = manage(new Image(StockID("gtk-close"), IconSize(ICON_SIZE_MENU)));
#if _USE_BUTTON_TAB
	m_tabImageBox = manage(new HBox());
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
	m_tabButton->set_alignment(0, 0);
	// Resize the button
	if (gotDimensions == true)
	{
		m_tabButton->set_size_request(width + 4, height + 4);
	}
	m_tabImageBox->pack_start(*m_tabImage, false, true);
	m_tabButton->add(*m_tabImageBox);
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
	m_tabImageBox->show();
	m_tabButton->show();
#else
	m_tabEventBox->show();
#endif
	show();

#if _USE_BUTTON_TAB
	m_tabButton->signal_clicked().connect(
		SigC::slot(*this, &NotebookTabBox::onButtonClicked));
#else
	m_tabEventBox->signal_button_press_event().connect(
		SigC::slot(*this, &NotebookTabBox::onButtonPressEvent));
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
Signal2<void, ustring, NotebookPageBox::PageType>& NotebookTabBox::getCloseSignal(void)
{
	return m_signalClose;
}
