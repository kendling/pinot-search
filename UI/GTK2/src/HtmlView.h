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

#ifndef _HTMLVIEW_HH
#define _HTMLVIEW_HH

#include <sigc++/object.h>
#include <gtkmm/menu.h>

#include "MozillaRenderer.h"

class HtmlView : public MozillaRenderer, public SigC::Object
{
	public:
		HtmlView(Gtk::Menu *pPopupMenu);
		virtual ~HtmlView();

		/// Returns the underlying widget.
		Gtk::Widget *getWidget(void) const;

	protected:
		Gtk::Menu *m_pPopupMenu;
		Gtk::Widget *m_pDocHtmlView;

		/// Handles button presses.
		void onButtonPressEvent(GdkEventButton *ev);

	private:
		HtmlView(const HtmlView &other);
		HtmlView &operator=(const HtmlView &other);

};

#endif // _HTMLVIEW_HH
