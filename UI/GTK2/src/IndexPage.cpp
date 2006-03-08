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
#include <gtkmm/alignment.h>
#include <gtkmm/buttonbox.h>
#include <gtkmm/stock.h>
#include <gtkmm/textbuffer.h>

#include "HtmlTokenizer.h"
#include "QueryHistory.h"
#include "ViewHistory.h"
#include "config.h"
#include "NLS.h"
#include "PinotSettings.h"
#include "PinotUtils.h"
#include "IndexPage.h"

using namespace std;
using namespace SigC;
using namespace Glib;
using namespace Gtk;

IndexPage::IndexPage(const ustring &indexName, IndexTree *pTree,
	PinotSettings &settings) :
	NotebookPageBox(indexName, NotebookPageBox::INDEX_PAGE, settings),
	m_pTree(pTree),
	m_pLabelCombobox(NULL),
	m_pBackButton(NULL),
	m_pForwardButton(NULL),
	m_docsCount(0),
	m_firstDoc(0)
{
	m_pLabelCombobox = manage(new ComboBox());

	Image *image521 = manage(new Image(StockID("gtk-media-rewind"), IconSize(4)));
	Label *label52 = manage(new Label(_("Show Previous")));
	HBox *hbox45 = manage(new HBox(false, 2));
	Alignment *alignment31 = manage(new Alignment(0.5, 0.5, 0, 0));
	m_pBackButton = manage(new Button());

	Image *image522 = manage(new Image(StockID("gtk-media-forward"), IconSize(4)));
	Label *label53 = manage(new Label(_("Show Next")));
	HBox *hbox46 = manage(new HBox(false, 2));
	Alignment *alignment32 = manage(new Alignment(0.5, 0.5, 0, 0));
	m_pForwardButton = manage(new Button());

	HButtonBox *indexHbuttonbox = manage(new HButtonBox(BUTTONBOX_START, 0));
	HBox *indexButtonsHbox = manage(new HBox(false, 0));

	// Buttons
	image521->set_alignment(0.5,0.5);
	image521->set_padding(0,0);
	label52->set_alignment(0.5,0.5);
	label52->set_padding(0,0);
	label52->set_justify(Gtk::JUSTIFY_LEFT);
	label52->set_line_wrap(false);
	label52->set_use_markup(false);
	label52->set_selectable(false);
	hbox45->pack_start(*image521, Gtk::PACK_SHRINK, 0);
	hbox45->pack_start(*label52, Gtk::PACK_SHRINK, 0);
	alignment31->add(*hbox45);
	m_pBackButton->set_flags(Gtk::CAN_FOCUS);
	m_pBackButton->set_flags(Gtk::CAN_DEFAULT);
	m_pBackButton->set_relief(Gtk::RELIEF_NORMAL);
	m_pBackButton->add(*alignment31);
	image522->set_alignment(0.5,0.5);
	image522->set_padding(0,0);
	label53->set_alignment(0.5,0.5);
	label53->set_padding(0,0);
	label53->set_justify(Gtk::JUSTIFY_LEFT);
	label53->set_line_wrap(false);
	label53->set_use_markup(false);
	label53->set_selectable(false);
	hbox46->pack_start(*image522, Gtk::PACK_SHRINK, 0);
	hbox46->pack_start(*label53, Gtk::PACK_SHRINK, 0);
	alignment32->add(*hbox46);
	m_pForwardButton->set_flags(Gtk::CAN_FOCUS);
	m_pForwardButton->set_flags(Gtk::CAN_DEFAULT);
	m_pForwardButton->set_relief(Gtk::RELIEF_NORMAL);
	m_pForwardButton->add(*alignment32);

	// Position everything
	indexHbuttonbox->pack_start(*m_pBackButton);
	indexHbuttonbox->pack_start(*m_pForwardButton);
	indexButtonsHbox->pack_start(*m_pLabelCombobox, Gtk::PACK_SHRINK, 4);
	indexButtonsHbox->pack_start(*indexHbuttonbox, Gtk::PACK_EXPAND_WIDGET, 4);
	pack_start(*indexButtonsHbox, Gtk::PACK_SHRINK, 4);
	if (pTree != NULL)
	{
		pack_start(*pTree->getScrolledWindow());
	}

	// Associate the columns model to the label combo
	m_refLabelNameTree = ListStore::create(m_labelNameColumns);
	m_pLabelCombobox->set_model(m_refLabelNameTree);
	m_pLabelCombobox->pack_start(m_labelNameColumns.m_name);
	// Populate
	populateLabelCombobox();

	// Connect the signals
	m_pLabelCombobox->signal_changed().connect(
		SigC::slot(*this, &IndexPage::onLabelChanged));
	m_pBackButton->signal_clicked().connect(
		SigC::slot(*this, &IndexPage::onBackClicked));
	m_pForwardButton->signal_clicked().connect(
		SigC::slot(*this, &IndexPage::onForwardClicked));

	// Disable the buttons until something is being shown
	m_pBackButton->set_sensitive(false);
	m_pForwardButton->set_sensitive(false);

	// Show all
	m_pLabelCombobox->show();
	image521->show();
	label52->show();
	hbox45->show();
	alignment31->show();
	m_pBackButton->show();
	image522->show();
	label53->show();
	hbox46->show();
	alignment32->show();
	m_pForwardButton->show();
	indexHbuttonbox->show();
	indexButtonsHbox->show();
	show();
}

IndexPage::~IndexPage()
{
}

void IndexPage::onLabelChanged(void)
{
	TreeModel::iterator labelIter = m_pLabelCombobox->get_active();
	if (labelIter)
	{
		TreeModel::Row row = *labelIter;

		m_labelName = row[m_labelNameColumns.m_name];
#ifdef DEBUG
		cout << "IndexPage::onLabelChanged: current label now " << m_labelName << endl;
#endif
		if (m_labelName == _("All labels"))
		{
			m_labelName.clear();
		}
		m_signalLabelChanged(m_title, m_labelName);
	}
}

void IndexPage::onBackClicked(void)
{
	m_signalBackClicked(m_title);
}

void IndexPage::onForwardClicked(void)
{
	m_signalForwardClicked(m_title);
}

//
// Returns the page's tree.
//
IndexTree *IndexPage::getTree(void) const
{
	return m_pTree;
}

//
// Returns the name of the current label.
//
ustring IndexPage::getLabelName(void) const
{
	return m_labelName;
}

//
// Populates the labels list.
//
void IndexPage::populateLabelCombobox(void)
{
	TreeModel::iterator iter;
	TreeModel::Row row;

	m_refLabelNameTree->clear();

	iter = m_refLabelNameTree->append();
	row = *iter;
	row[m_labelNameColumns.m_name] = _("All labels");

	const set<string> &labels = m_settings.getLabels();
	for (set<string>::const_iterator labelIter = labels.begin();
		labelIter != labels.end(); ++labelIter)
	{
		string labelName(*labelIter);

		iter = m_refLabelNameTree->append();
		row = *iter;
		row[m_labelNameColumns.m_name] = to_utf8(labelName);
	}

	m_pLabelCombobox->set_active(0);
}

//
// Updates the state of the index buttons.
//
void IndexPage::updateButtonsState(unsigned int maxDocsCount)
{
#ifdef DEBUG
	cout << "IndexPage::updateButtonsState: counts " << m_firstDoc
		<< " " << m_docsCount << endl;
#endif
	if (m_firstDoc >= maxDocsCount)
	{
		m_pBackButton->set_sensitive(true);
	}
	else
	{
		m_pBackButton->set_sensitive(false);
	}
	if (m_docsCount > m_firstDoc + maxDocsCount)
	{
		m_pForwardButton->set_sensitive(true);
	}
	else
	{
		m_pForwardButton->set_sensitive(false);
	}
}

//
// Gets the number of documents.
//
unsigned int IndexPage::getDocumentsCount(void) const
{
	return m_docsCount;
}

//
// Sets the number of documents.
//
void IndexPage::setDocumentsCount(unsigned int docsCount)
{
	m_docsCount = docsCount;
}

//
// Gets the first document.
//
unsigned int IndexPage::getFirstDocument(void) const
{
	return m_firstDoc;
}

//
// Sets the first document.
//
void IndexPage::setFirstDocument(unsigned int startDoc)
{
	m_firstDoc = startDoc;
}

//
// Returns the changed label signal.
//
Signal2<void, ustring, ustring>& IndexPage::getLabelChangedSignal(void)
{
	return m_signalLabelChanged;
}

//
// Returns the back button clicked signal.
//
Signal1<void, ustring>& IndexPage::getBackClickedSignal(void)
{
	return m_signalBackClicked;
}

//
// Returns the forward button clicked signal.
//
Signal1<void, ustring>& IndexPage::getForwardClickedSignal(void)
{
	return m_signalForwardClicked;
}
