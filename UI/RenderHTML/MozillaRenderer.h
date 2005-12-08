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

#ifndef _MOZILLARENDERER_H
#define _MOZILLARENDERER_H

#include <string>
#include <gtk/gtkwidget.h>

using namespace std;

/**
  * A class that renders HTML using gtk-mozembed.
  * See documentation at http://www.mozilla.org/unix/gtk-embedding.html
  */
class MozillaRenderer
{
	public:
		MozillaRenderer();
		virtual ~MozillaRenderer();

		/// Returns the GTK widget.
		GtkWidget *getWidget(void);

		/// Renders HTML held in a buffer.
		bool renderData(const char *data, unsigned int length, const string &baseUrl);

		/// Renders the given URL.
		bool renderUrl(const string &url);

		/// Returns the current location.
		string getLocation(void);

		/// Returns the currently displayed document's title.
		string getTitle(void);

		/// Goes forward.
		bool goForward(void);

		/// Goes back.
		bool goBack(void);

		/// Reloads the current page.
		bool reload(void);

		/// Stops loading.
		bool stop(void);

		/// Returns true if renderer can go forward.
		bool canGoForward(void);

		/// Returns true if renderer can go back.
		bool canGoBack(void);

	protected:
		GtkWidget *m_htmlWidget;
		bool m_rendering;

	private:
		MozillaRenderer(const MozillaRenderer &other);
		MozillaRenderer &operator=(const MozillaRenderer &other);

};

#endif // _MOZILLARENDERER_H
