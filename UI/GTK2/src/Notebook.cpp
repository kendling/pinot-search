/*
 *  Copyright 2005-2008 Fabrice Colin
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <gtkmm/rc.h>

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
	m_pLabel(NULL),
	m_pCombobox(NULL),
	m_pButton(NULL),
	m_pHBox(NULL),
	m_pVBox(NULL),
	m_pVPaned(NULL),
	m_pTree(pTree)
{
	if (pTree != NULL)
	{
		m_pLabel = manage(new Label(_("Did you mean ?")));
		m_pCombobox = manage(new ComboBoxText());
		m_pButton = manage(new Button(StockID("gtk-yes")));

		m_pHBox = manage(new HBox(false, 0));
		m_pHBox->pack_start(*m_pLabel, Gtk::PACK_SHRINK, 4);
		m_pHBox->pack_start(*m_pCombobox, Gtk::PACK_EXPAND_WIDGET, 4);
		m_pHBox->pack_start(*m_pButton, Gtk::PACK_SHRINK, 4);

		m_pVBox = manage(new VBox(false, 0));
		m_pVBox->pack_start(*pTree->getResultsScrolledWindow());
		m_pVBox->pack_start(*m_pHBox, Gtk::PACK_SHRINK, 0);

		m_pVPaned = manage(new VPaned());
		m_pVPaned->set_flags(Gtk::CAN_FOCUS);
		m_pVPaned->set_position(105);
		m_pVPaned->pack1(*m_pVBox, Gtk::EXPAND|Gtk::SHRINK);
		m_pVPaned->pack2(*pTree->getExtractScrolledWindow(), Gtk::SHRINK);
		pack_start(*m_pVPaned, Gtk::PACK_EXPAND_WIDGET, 0);

		// Give the extract 2/10th of the height
		m_pVPaned->set_position((parentHeight * 8) / 10);

		// Hide suggestions by default
		m_pLabel->hide();
		m_pCombobox->hide();
		m_pButton->hide();
		m_pHBox->hide();
		m_pVBox->show();
		m_pVPaned->show();

		m_pButton->signal_clicked().connect(
			sigc::mem_fun(*this, &ResultsPage::onButtonClicked), false);
	}

	show();
}

ResultsPage::~ResultsPage()
{
}

void ResultsPage::onButtonClicked()
{
	if (m_pCombobox == NULL)
	{
		return;
	}

	m_signalSuggest(m_title, m_pCombobox->get_active_text());
}

//
// Returns the page's tree.
//
ResultsTree *ResultsPage::getTree(void) const
{
	return m_pTree;
}

// Returns the suggest signal.
sigc::signal2<void, ustring, ustring>& ResultsPage::getSuggestSignal(void)
{
	return m_signalSuggest;
}

//
// Append a suggestion.
//
void ResultsPage::appendSuggestion(const ustring &text)
{
	if (text.empty() == false)
	{
		m_pCombobox->prepend_text(text);
		m_pCombobox->set_active(0);

		m_pLabel->show();
		m_pCombobox->show();
		m_pButton->show();
		m_pHBox->show();
	}
}

bool NotebookTabBox::m_initialized = false;

NotebookTabBox::NotebookTabBox(const Glib::ustring &title, NotebookPageBox::PageType type) :
	HBox(),
	m_title(title),
	m_pageType(type),
	m_tabLabel(NULL),
	m_tabImage(NULL),
	m_tabButton(NULL)
{
	if (m_initialized == false)
	{
		m_initialized = true;

		// This was lifted from gnome-terminal's terminal-window.c
		RC::parse_string("style \"pinot-tab-close-button-style\"\n"
			"{\n"
			"GtkWidget::focus-padding = 0\n"
			"GtkWidget::focus-line-width = 0\n"
			"xthickness = 0\n"
			"ythickness = 0\n"
			"}\n"
			"widget \"*.pinot-tab-close-button\" style \"pinot-tab-close-button-style\"");
	}

	m_tabLabel = manage(new Label(title));
	m_tabImage = manage(new Image(StockID("gtk-close"), IconSize(ICON_SIZE_MENU)));
	m_tabButton = manage(new Button());

	m_tabLabel->set_alignment(0, 0.5);
	m_tabLabel->set_padding(0, 0);
	m_tabLabel->set_justify(JUSTIFY_LEFT);
	m_tabLabel->set_line_wrap(false);
	m_tabLabel->set_use_markup(false);
	m_tabLabel->set_selectable(false);
	m_tabImage->set_alignment(0, 0);
	m_tabImage->set_padding(0, 0);
	m_tabButton->set_relief(RELIEF_NONE);
	m_tabButton->set_border_width(0);
	m_tabButton->set_name("pinot-tab-close-button");
	m_tabButton->set_tooltip_text(_("Close"));
	m_tabButton->set_alignment(0, 0);
	m_tabButton->add(*m_tabImage);
	pack_start(*m_tabLabel);
	pack_start(*m_tabButton, PACK_SHRINK);
	set_spacing(0);
	set_homogeneous(false);
	m_tabLabel->show();
	m_tabImage->show();
	m_tabButton->show();
	show();

	m_tabButton->signal_clicked().connect(
		sigc::mem_fun(*this, &NotebookTabBox::onButtonClicked));
}

NotebookTabBox::~NotebookTabBox()
{
}

void NotebookTabBox::onButtonClicked(void)
{
	m_signalClose(m_title, m_pageType);
}

//
// Returns the close signal.
//
sigc::signal2<void, ustring, NotebookPageBox::PageType>& NotebookTabBox::getCloseSignal(void)
{
	return m_signalClose;
}
