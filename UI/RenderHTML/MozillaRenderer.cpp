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
#include <nspr/nspr.h>
#include <nss/nss.h>
#include <nss/ssl.h>
#include <gtkmozembed.h>

#include "MozillaRenderer.h"

MozillaRenderer::MozillaRenderer()
{
	// Initialize NSPR and NSS
	PR_Init (PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 10);
	if (NSS_InitReadWrite(NULL) == SECFailure)
	{
		if (NSS_NoDB_Init(NULL) == SECFailure)
		{
#ifdef DEBUG
			cout << "MozillaRenderer::ctor: couldn't initialize NSS" << endl;
#endif
		}
	}
	NSS_SetDomesticPolicy();
	SSL_OptionSetDefault(SSL_ENABLE_SSL2, PR_TRUE);
	SSL_OptionSetDefault(SSL_ENABLE_SSL3, PR_TRUE);
	SSL_OptionSetDefault(SSL_ENABLE_TLS, PR_TRUE);
	SSL_OptionSetDefault(SSL_V2_COMPATIBLE_HELLO, PR_TRUE);

	gtk_moz_embed_push_startup();

	// Create our web browser component
	m_htmlWidget = gtk_moz_embed_new();
	if (m_htmlWidget != NULL)
	{
		// Turn off all features
		gtk_moz_embed_set_chrome_mask(GTK_MOZ_EMBED(m_htmlWidget), 0);
#ifdef DEBUG
		cout << "MozillaRenderer::ctor: embedded Mozilla" << endl;
#endif
	}
#ifdef DEBUG
	else cerr << "MozillaRenderer::ctor: failed to embed Mozilla" << endl;
#endif
	m_rendering = false;
}

MozillaRenderer::~MozillaRenderer()
{
	gtk_moz_embed_pop_startup();
	// Shutdown NSS and NSPR
	NSS_Shutdown();
	// FIXME: this hangs, waiting on a condition variable
	//PR_Cleanup();
}

/// Returns the GTK widget.
GtkWidget *MozillaRenderer::getWidget(void)
{
	return m_htmlWidget;
}

/// Renders HTML held in a buffer.
bool MozillaRenderer::renderData(const char *data, unsigned int length, const string &baseUrl)
{
	if ((m_htmlWidget == NULL) ||
		(data == NULL) ||
		(length == 0))
	{
		return false;
	}

#ifdef DEBUG
	cout << "MozillaRenderer::renderData: data length " << strlen(data) << "/" << length << " at " << baseUrl << endl;
#endif
	gtk_moz_embed_open_stream(GTK_MOZ_EMBED(m_htmlWidget), baseUrl.c_str(), "text/html");
	// Append data to stream in medium-sized chunks
	const char *where = data;
	unsigned int total = length, chunk = 0;
	while (total > 0)
	{
		if (total > 15000)
		{
			chunk = 15000;
		}
		else
		{
			chunk = total;
		}
#ifdef DEBUG
		cout << "MozillaRenderer::renderData: appending " << chunk << " to stream" << endl;
#endif
		gtk_moz_embed_append_data(GTK_MOZ_EMBED(m_htmlWidget), where, chunk);

		where += chunk;
		total = total - chunk;
	}
	gtk_moz_embed_close_stream(GTK_MOZ_EMBED(m_htmlWidget));
	m_rendering = true;

	return true;
}

/// Renders the given URL.
bool MozillaRenderer::renderUrl(const string &url)
{
	if (m_htmlWidget != NULL)
	{
#ifdef DEBUG
		cout << "MozillaRenderer::renderUrl: url " << url << endl;
#endif
		gtk_moz_embed_load_url(GTK_MOZ_EMBED(m_htmlWidget), url.c_str());
		m_rendering = true;
	}

	return m_rendering;
}

/// Returns the current location.
string MozillaRenderer::getLocation(void)
{
	if ((m_htmlWidget == NULL) ||
		(m_rendering == false))
	{
		return "";
	}

	char *location = gtk_moz_embed_get_location(GTK_MOZ_EMBED(m_htmlWidget));
	if (location == NULL)
	{
		return "";
	}
	string locationStr = location;
	free(location);

	return locationStr;
}

/// Returns the currently displayed document's title.
string MozillaRenderer::getTitle(void)
{
	if ((m_htmlWidget == NULL) ||
		(m_rendering == false))
	{
		return "";
	}

	char *title = gtk_moz_embed_get_title(GTK_MOZ_EMBED(m_htmlWidget));
	if (title == NULL)
	{
		return "";
	}
	string titleStr = title;
	free(title);

	return titleStr;
}

/// Goes forward.
bool MozillaRenderer::goForward(void)
{
	if (canGoForward() == true)
	{
		gtk_moz_embed_go_forward(GTK_MOZ_EMBED(m_htmlWidget));

		return true;
	}

	return false;
}

/// Goes back.
bool MozillaRenderer::goBack(void)
{
	if (canGoForward() == true)
	{
		gtk_moz_embed_go_back(GTK_MOZ_EMBED(m_htmlWidget));

		return true;
	}

	return false;
}

/// Reloads the current page.
bool MozillaRenderer::reload(void)
{
	if ((m_htmlWidget == NULL) ||
		(m_rendering == false))
	{
		return false;
	}

	gtk_moz_embed_reload(GTK_MOZ_EMBED(m_htmlWidget), GTK_MOZ_EMBED_FLAG_RELOADNORMAL);

	return true;	
}

/// Stops loading.
bool MozillaRenderer::stop(void)
{
	if ((m_htmlWidget == NULL) ||
		(m_rendering == false))
	{
		return false;
	}

	gtk_moz_embed_stop_load(GTK_MOZ_EMBED(m_htmlWidget));

	return true;	
}

bool MozillaRenderer::canGoForward(void)
{
	if ((m_htmlWidget == NULL) ||
		(m_rendering == false))
	{
		return false;
	}

	return gtk_moz_embed_can_go_forward(GTK_MOZ_EMBED(m_htmlWidget));
}

bool MozillaRenderer::canGoBack(void)
{
	if ((m_htmlWidget == NULL) ||
		(m_rendering == false))
	{
		return false;
	}

	return gtk_moz_embed_can_go_back(GTK_MOZ_EMBED(m_htmlWidget));
}
