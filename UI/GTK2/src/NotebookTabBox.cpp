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

#include "NotebookTabBox.h"
#include "PinotUtils.h"

using namespace SigC;
using namespace Glib;
using namespace Gtk;

NotebookTabBox::NotebookTabBox(const Glib::ustring &title, PageType type) :
	HBox(),
	m_title(title),
	m_pageType(type),
	m_tabLabel(NULL),
#if _USE_BUTTON_TAB
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
	m_tabButton = manage(new Button());
#else
	m_tabEventBox = manage(new EventBox);
#endif

	m_tabLabel->set_alignment(0,0.5);
	m_tabLabel->set_padding(0,0);
	m_tabLabel->set_justify(JUSTIFY_LEFT);
	m_tabLabel->set_line_wrap(false);
	m_tabLabel->set_use_markup(false);
	m_tabLabel->set_selectable(false);
	m_tabImage->set_alignment(0,0);
	m_tabImage->set_padding(0,0);
#if _USE_BUTTON_TAB
	m_tabButton->set_flags(CAN_FOCUS);
	m_tabButton->set_flags(CAN_DEFAULT);
	m_tabButton->add(*m_tabImage);
	m_tabButton->set_relief(RELIEF_NONE);
	m_tabButton->set_border_width(0);
	m_tabButton->set_alignment(0,0);
	// Resize the button
	if (gotDimensions == true)
	{
		m_tabButton->set_size_request(width, height);
		m_tabButton->check_resize();
	}
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

	return true;
}

//
// Returns the page type.
//
NotebookTabBox::PageType NotebookTabBox::getPageType(void) const
{
	return m_pageType;
}

//
// Returns the close signal.
//
Signal2<void, ustring, NotebookTabBox::PageType>& NotebookTabBox::getCloseSignal(void)
{
	return m_signalClose;
}
