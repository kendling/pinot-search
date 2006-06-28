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
#include <glibmm/ustring.h>
#include <glibmm/convert.h>
#include <gtkmm/menu.h>

#include "config.h"
#include "Languages.h"
#include "NLS.h"
#include "PinotUtils.h"
#include "queryDialog.hh"

using namespace std;
using namespace Glib;
using namespace Gtk;

queryDialog::queryDialog(QueryProperties &properties) :
	queryDialog_glade(),
	m_name(properties.getName()),
	m_properties(properties),
	m_labels(PinotSettings::getInstance().getLabels()),
	m_badName(true)
{
	string name = m_properties.getName();

	// Associate the columns model to the index label combo
	m_refLabelNameTree = ListStore::create(m_labelNameColumns);
	labelNameCombobox->set_model(m_refLabelNameTree);
	labelNameCombobox->pack_start(m_labelNameColumns.m_name);
	// ...and the label filter combo
	m_refLabelFilterTree = ListStore::create(m_labelFilterColumns);
	labelFilterCombobox->set_model(m_refLabelFilterTree);
	labelFilterCombobox->pack_start(m_labelFilterColumns.m_name);
	// Associate the columns model to the language combo
	m_refLanguageTree = ListStore::create(m_languageColumns);
	languageCombobox->set_model(m_refLanguageTree);
	languageCombobox->pack_start(m_languageColumns.m_name);
	// Populate
	populate_comboboxes();

	// Name
	if (name.empty() == true)
	{
		queryOkbutton->set_sensitive(false);
	}
	else
	{
		nameEntry->set_text(to_utf8(name));
	}
	// Query terms
	andEntry->set_text(to_utf8(m_properties.getAndWords()));
	phraseEntry->set_text(to_utf8(m_properties.getPhrase()));
	anyEntry->set_text(to_utf8(m_properties.getAnyWords()));
	notEntry->set_text(to_utf8(m_properties.getNotWords()));

	// Host name
	hostNameEntry->set_text(to_utf8(m_properties.getHostFilter()));
	// File name
	fileNameEntry->set_text(to_utf8(m_properties.getFileFilter()));
	// Maximum number of results
	resultsCountSpinbutton->set_value((double)m_properties.getMaximumResultsCount());
	// Index all results
	indexCheckbutton->set_active(m_properties.getIndexResults());
}

queryDialog::~queryDialog()
{
}

void queryDialog::populate_comboboxes()
{
	unsigned int labelNum = 1;

	TreeModel::iterator iter = m_refLabelNameTree->append();
	TreeModel::Row row = *iter;
	row[m_labelNameColumns.m_name] = _("None");
	labelNameCombobox->set_active(0);

	iter = m_refLabelFilterTree->append();
	row = *iter;
	row[m_labelFilterColumns.m_name] = _("Any");
	labelFilterCombobox->set_active(0);

	// Add all labels to both label combos and select the one defined for the query
	for (set<string>::const_iterator labelIter = m_labels.begin(); labelIter != m_labels.end(); ++labelIter)
	{
		string labelName(*labelIter);

		iter = m_refLabelNameTree->append();
		row = *iter;
		row[m_labelNameColumns.m_name] = to_utf8(labelName);
		if (labelName == m_properties.getLabelName())
		{
			labelNameCombobox->set_active(labelNum);
		}

		iter = m_refLabelFilterTree->append();
		row = *iter;
		row[m_labelFilterColumns.m_name] = to_utf8(labelName);
		if (labelName == m_properties.getLabelFilter())
		{
			labelFilterCombobox->set_active(labelNum);
		}

		++labelNum;
	}

	iter = m_refLanguageTree->append();
	row = *iter;
	row[m_languageColumns.m_name] = _("Any");
	languageCombobox->set_active(0);

	// Add all supported languages and select the one defined for the query
	for (unsigned int languageNum = 0; languageNum < Languages::m_count; ++languageNum)
	{
		string languageName = Languages::getIntlName(languageNum);
		iter = m_refLanguageTree->append();
		row = *iter;
		row[m_languageColumns.m_name] = to_utf8(languageName);

		if (languageName == m_properties.getLanguage())
		{
			languageCombobox->set_active(languageNum + 1);
		}
	}
}

bool queryDialog::badName(void) const
{
	return m_badName;
}

void queryDialog::on_queryOkbutton_clicked()
{
	// Name
	m_properties.setName(from_utf8(nameEntry->get_text()));
	m_badName = false;
	// Did the name change ?
	if (m_name != m_properties.getName())
	{
		const std::map<string, QueryProperties> &queries = PinotSettings::getInstance().getQueries();

		// Is it already used ?
		std::map<string, QueryProperties>::const_iterator queryIter = queries.find(m_properties.getName());
		if (queryIter != queries.end())
		{
			// Yes, it is
			m_badName = true;
#ifdef DEBUG
			cout << "queryDialog::on_queryOkbutton_clicked: name in use" << endl;
#endif
		}
	}

	// Query terms
	m_properties.setAndWords(from_utf8(andEntry->get_text()));
	m_properties.setPhrase(from_utf8(phraseEntry->get_text()));
	m_properties.setAnyWords(from_utf8(anyEntry->get_text()));
	m_properties.setNotWords(from_utf8(notEntry->get_text()));
	// Language
	m_properties.setLanguage("");
	int chosenLanguage = languageCombobox->get_active_row_number();
	if (chosenLanguage > 0)
	{
		TreeModel::iterator iter = languageCombobox->get_active();
		TreeModel::Row row = *iter;
		string languageName = from_utf8(row[m_languageColumns.m_name]);

		m_properties.setLanguage(languageName);
	}
	// Maximum number of results
	m_properties.setMaximumResultsCount((unsigned int)resultsCountSpinbutton->get_value());
	// Index all results
	m_properties.setIndexResults(indexCheckbutton->get_active());
	// Index label
	m_properties.setLabelName("");
	int chosenLabel = labelNameCombobox->get_active_row_number();
	if (chosenLabel > 0)
	{
		TreeModel::iterator iter = labelNameCombobox->get_active();
		TreeModel::Row row = *iter;
		string labelName = from_utf8(row[m_labelNameColumns.m_name]);

		m_properties.setLabelName(labelName);
	}
	// Filters
	m_properties.setHostFilter(from_utf8(hostNameEntry->get_text()));
	m_properties.setFileFilter(from_utf8(fileNameEntry->get_text()));
	// Label filter
	m_properties.setLabelFilter("");
	chosenLabel = labelFilterCombobox->get_active_row_number();
	if (chosenLabel > 0)
	{
		TreeModel::iterator iter = labelFilterCombobox->get_active();
		TreeModel::Row row = *iter;
		string labelName = from_utf8(row[m_labelFilterColumns.m_name]);

		m_properties.setLabelFilter(labelName);
	}

	// Workaround for bizarre bug that would cause a crash when creating a query
	// that indexes and labels results based on a language filter
	if (queryNotebook->get_current_page() == 0)
	{
		queryNotebook->next_page();
	}
	else
	{
		queryNotebook->prev_page();
		queryNotebook->next_page();
	}
}

void queryDialog::on_nameEntry_changed()
{
	ustring name = nameEntry->get_text();
	if (name.empty() == false)
	{
		queryOkbutton->set_sensitive(true);
	}
	else
	{
		queryOkbutton->set_sensitive(false);
	}
}
