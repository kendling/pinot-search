// generated 2004/8/13 22:59:43 BST by fabrice@amra.dyndns.org.(none)
// using glademm V2.6.0_cvs
//
// newer (non customized) versions of this file go to propertiesDialog.cc_new

// This file is for your program, I won't touch it again!

#include <iostream>
#include <glibmm/convert.h>
#include <pangomm/font.h>
#include <gtkmm/rc.h>

#include "config.h"
#include "NLS.h"
#include "PinotSettings.h"
#include "PinotUtils.h"
#include "propertiesDialog.hh"

using namespace std;
using namespace Glib;
using namespace Gdk;
using namespace Gtk;

propertiesDialog::propertiesDialog(const std::set<std::string> &docLabels,
	const DocumentInfo &docInfo, bool editDocument) :
	propertiesDialog_glade(),
	m_editDocument(editDocument),
	m_docInfo(docInfo)
{
	// Associate the columns model to the labels tree
	m_refLabelsTree = ListStore::create(m_labelsColumns);
	labelsTreeview->set_model(m_refLabelsTree);
	labelsTreeview->append_column_editable(" ", m_labelsColumns.m_enabled);
	labelsTreeview->append_column(_("Label"), m_labelsColumns.m_name);
	// Allow only single selection
	labelsTreeview->get_selection()->set_mode(SELECTION_SINGLE);

	if (m_editDocument == true)
	{
		ustring language = to_utf8(docInfo.getLanguage());

		titleEntry->set_text(to_utf8(docInfo.getTitle()));
		if (language.empty() == true)
		{
			language = _("Unknown");
		}
		languageEntry->set_text(language);
		typeEntry->set_text(to_utf8(docInfo.getType()));
	}
	else
	{
		titleLabel->hide();
		titleEntry->hide();
		languageLabel->hide();
		languageEntry->hide();
		typeLabel->hide();
		typeEntry->hide();
	}
	// FIXME: get the extract
	extractLabel->hide();
	extractScrolledwindow->hide();

	populate_labelsTreeview(docLabels);
}

propertiesDialog::~propertiesDialog()
{
}

void propertiesDialog::populate_labelsTreeview(const set<string> &docLabels)
{
	TreeModel::iterator iter;
	TreeModel::Row row;

	// Populate the tree
	const set<PinotSettings::Label> &sysLabels = PinotSettings::getInstance().m_labels;
	for (set<PinotSettings::Label>::const_iterator labelIter = sysLabels.begin(); labelIter != sysLabels.end(); ++labelIter)
	{
		// Create a new row
		iter = m_refLabelsTree->append();
		row = *iter;

		row[m_labelsColumns.m_name] = labelIter->m_name;
		string labelName = locale_from_utf8(labelIter->m_name);
		// Is it in the document labels list ?
		set<string>::const_iterator iter = find(docLabels.begin(), docLabels.end(), labelName);
		if (iter != docLabels.end())
		{
			// Yup
			row[m_labelsColumns.m_enabled] = true;
		}
		else
		{
			row[m_labelsColumns.m_enabled] = false;
		}
	}
}

void propertiesDialog::setHeight(int maxHeight)
{
	// FIXME: there must be a better way to determine how high the tree should be
	// for all rows to be visible !
	int labelsCount = m_refLabelsTree->children().size();
	// By default, the tree is high enough for two rows to be visible
	if (labelsCount > 2)
	{
		int width, height;
		get_size(width, height);

		RefPtr<Style> refRCStyle = RC::get_style(*labelsTreeview);
		int fontSize = refRCStyle->get_font().get_size() / Pango::SCALE;
#ifdef DEBUG
		cout << "propertiesDialog::setHeight: max " << maxHeight << ", dialog " << height
			<< ", font " << fontSize << " " << refRCStyle->get_font().get_size() << endl;
#endif
		height += fontSize * (labelsCount - 2);

		TreeViewColumn *pColumn = labelsTreeview->get_column(1);
		if (pColumn != NULL)
		{
			Rectangle cell_area;
			int x_offset, y_offset, cellWidth, cellHeight;
			pColumn->cell_get_size(cell_area, x_offset, y_offset, cellWidth, cellHeight);
#ifdef DEBUG
			cout << "propertiesDialog::setHeight: cell " << cellHeight << " " << y_offset << endl;
#endif
			height += cellHeight * (labelsCount - 2);
		}
#ifdef DEBUG
		cout << "propertiesDialog::setHeight: dialog " << height << endl;
#endif

		if (height > maxHeight)
		{
			height = maxHeight;
		}
		resize(width, height);
	}
}

const DocumentInfo &propertiesDialog::getDocumentInfo(void)
{
	return m_docInfo;
}

const set<string> &propertiesDialog::getLabels(void) const
{
	return m_labels;
}

void propertiesDialog::on_labelOkButton_clicked()
{
	if (m_editDocument == true)
	{
		// Title
		m_docInfo.setTitle(locale_from_utf8(titleEntry->get_text()));
	}
	// Go through the labels tree
	TreeModel::Children children = m_refLabelsTree->children();
	if (children.empty() == false)
	{
		for (TreeModel::Children::iterator iter = children.begin(); iter != children.end(); ++iter)
		{
			TreeModel::Row row = *iter;

			bool enabled = row[m_labelsColumns.m_enabled];
			if (enabled == true)
			{
				ustring labelName = row[m_labelsColumns.m_name];
				m_labels.insert(locale_from_utf8(labelName));
			}
		}
	}
}
