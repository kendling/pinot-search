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

#include <iostream>

#include <glibmm/wrap.h>

#include "config.h"
#include "NLS.h"
#include "HtmlView.h"
#include "PinotUtils.h"

using namespace Glib;
using namespace Gtk;

HtmlView::HtmlView(VBox *viewVbox, Menu *pPopupMenu) :
	MozillaRenderer(),
	m_pPopupMenu(pPopupMenu)
{
	GtkWidget *view = getWidget();
	if (view != NULL)
	{
		// Wrap this into a gtkmm Widget object
		// FIXME: need manage(wrap()) ?
		Widget *pDocHtmlView = wrap(view);
		if ((pDocHtmlView != NULL) &&
			(viewVbox != NULL))
		{
			viewVbox->pack_start(*pDocHtmlView);

			// Handle button presses
			pDocHtmlView->signal_button_press_event().connect_notify(SigC::slot(*this, &HtmlView::onButtonPressEvent));

			pDocHtmlView->show();
		}
	}
}

HtmlView::~HtmlView()
{
}

//
// Handles button presses.
//
void HtmlView::onButtonPressEvent(GdkEventButton *ev)
{
#ifdef DEBUG
	cout << "HtmlView::onButtonPressEvent: click on button " << ev->button << endl;
#endif
	// Check for popup click
	if ((ev->type == GDK_BUTTON_PRESS) &&
		(ev->button == 3) )
	{
		if (m_pPopupMenu != NULL)
		{
			m_pPopupMenu->popup(ev->button, ev->time);
		}
	}
}
