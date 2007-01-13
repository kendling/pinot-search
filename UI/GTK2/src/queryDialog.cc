/*
 *  Copyright 2005,2006 Fabrice Colin
 *
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
#include "TimeConverter.h"
#include "PinotUtils.h"
#include "queryDialog.hh"
#include "dateDialog.hh"

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
	unsigned int day, month, year;

	// Name
	string name(m_properties.getName());
	if (name.empty() == true)
	{
		queryOkbutton->set_sensitive(false);
	}
	else
	{
		nameEntry->set_text(to_utf8(name));
	}
	// Query text
	RefPtr<TextBuffer> refBuffer = queryTextview->get_buffer();
	refBuffer->set_text(to_utf8(m_properties.getFreeQuery()));
	// Maximum number of results
	resultsCountSpinbutton->set_value((double)m_properties.getMaximumResultsCount());
	// Date range
	bool enable = m_properties.getMinimumDate(day, month, year);
	if (year > 0)
	{
		m_fromDate.set_dmy((Date::Day )day, (Date::Month )month, (Date::Year )year);
	}
	else
	{
		m_fromDate.set_time(time(NULL));
	}
	fromCheckbutton->set_active(enable);
	fromButton->set_label(m_fromDate.format_string("%x"));
	enable = m_properties.getMaximumDate(day, month, year);
	if (year > 0)
	{
		m_toDate.set_dmy((Date::Day )day, (Date::Month )month, (Date::Year )year);
	}
	else
	{
		m_toDate.set_time(time(NULL));
	}
	toCheckbutton->set_active(enable);
	toButton->set_label(m_toDate.format_string("%x"));
	// Index all results
	indexCheckbutton->set_active(m_properties.getIndexResults());

	// Associate the columns model to the index label combo
	m_refLabelNameTree = ListStore::create(m_labelNameColumns);
	labelNameCombobox->set_model(m_refLabelNameTree);
	labelNameCombobox->pack_start(m_labelNameColumns.m_name);
	// ...and the filter combo
	m_refFilterTree = ListStore::create(m_filterColumns);
	filterCombobox->set_model(m_refFilterTree);
	filterCombobox->pack_start(m_filterColumns.m_name);
	// Populate
	populate_comboboxes();
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

	// Add all labels to the label combo and select the one defined for the query
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

		++labelNum;
	}

	// All supported filters
	iter = m_refFilterTree->append();
	row = *iter;
	row[m_filterColumns.m_name] = _("Host name");
	iter = m_refFilterTree->append();
	row = *iter;
	row[m_filterColumns.m_name] = _("File name");
	iter = m_refFilterTree->append();
	row = *iter;
	row[m_filterColumns.m_name] = _("File extension");
	iter = m_refFilterTree->append();
	row = *iter;
	row[m_filterColumns.m_name] = _("Title");
	iter = m_refFilterTree->append();
	row = *iter;
	row[m_filterColumns.m_name] = _("URL");
	iter = m_refFilterTree->append();
	row = *iter;
	row[m_filterColumns.m_name] = _("Directory");
	iter = m_refFilterTree->append();
	row = *iter;
	row[m_filterColumns.m_name] = _("Language code");
	iter = m_refFilterTree->append();
	row = *iter;
	row[m_filterColumns.m_name] = _("MIME type");
	iter = m_refFilterTree->append();
	row = *iter;
	row[m_filterColumns.m_name] = _("Label");
	filterCombobox->set_active(0);
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

	// Query text
	RefPtr<TextBuffer> refBuffer = queryTextview->get_buffer();
	m_properties.setFreeQuery(from_utf8(refBuffer->get_text()));
	// Maximum number of results
	m_properties.setMaximumResultsCount((unsigned int)resultsCountSpinbutton->get_value());
	// Date range
	m_properties.setMinimumDate(fromCheckbutton->get_active(), m_fromDate.get_day(), m_fromDate.get_month(), m_fromDate.get_year());
	m_properties.setMaximumDate(toCheckbutton->get_active(), m_toDate.get_day(), m_toDate.get_month(), m_toDate.get_year());
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

void queryDialog::on_addFilterButton_clicked()
{
	ustring filter;

	// What's the corresponding filter ?
	int chosenFilter = filterCombobox->get_active_row_number();
	// FIXME: should the filters be localized ?
	switch (chosenFilter)
	{
		case 0:
			filter = "site";
			break;
		case 1:
			filter = "file";
			break;
		case 2:
			filter = "ext";
			break;
		case 3:
			filter = "title";
			break;
		case 4:
			filter = "url";
			break;
		case 5:
			filter = "dir";
			break;
		case 6:
			filter = "lang";
			break;
		case 7:
			filter = "type";
			break;
		case 8:
			filter = "label";
			break;
		default:
			break;
	}

	RefPtr<TextBuffer> refBuffer = queryTextview->get_buffer();
	ustring queryText = refBuffer->get_text();
	queryText += " ";
	queryText += filter;
	queryText += ":xxx";
	refBuffer->set_text(queryText);
}

void queryDialog::on_fromButton_clicked()
{
	dateDialog datePicker(_("Start date"), m_fromDate);

#ifdef DEBUG
	cout << "queryDialog::on_fromButton_clicked: start date is " << m_fromDate.format_string("%x") << endl;
#endif
	datePicker.show();
	if (datePicker.run() == RESPONSE_OK)
	{
		datePicker.getChoice(m_fromDate);
		fromButton->set_label(m_fromDate.format_string("%x"));
	}
}

void queryDialog::on_toButton_clicked()
{
	dateDialog datePicker(_("End date"), m_toDate);

#ifdef DEBUG
	cout << "queryDialog::on_toButton_clicked: end date is " << m_toDate.format_string("%x") << endl;
#endif
	datePicker.show();
	if (datePicker.run() == RESPONSE_OK)
	{
		datePicker.getChoice(m_toDate);
	        toButton->set_label(m_toDate.format_string("%x"));
	}
}

