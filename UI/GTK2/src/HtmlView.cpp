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

HtmlView::HtmlView(Menu *pPopupMenu) :
	MozillaRenderer(),
	SigC::Object(),
	m_pPopupMenu(pPopupMenu),
	m_pDocHtmlView(NULL)
{
	GtkWidget *view = getObject();
	if (view != NULL)
	{
		// Wrap this into a gtkmm Widget object
		m_pDocHtmlView = wrap(view);
		if (m_pDocHtmlView != NULL)
		{
			// Handle button presses
			m_pDocHtmlView->signal_button_press_event().connect_notify(
				SigC::slot(*this, &HtmlView::onButtonPressEvent));

			m_pDocHtmlView->show();
		}
	}
}

HtmlView::~HtmlView()
{
	if (m_pDocHtmlView != NULL)
	{
		delete m_pDocHtmlView;
	}
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

//
// Returns the underlying widget.
//
Widget *HtmlView::getWidget(void) const
{
	return m_pDocHtmlView;
}
