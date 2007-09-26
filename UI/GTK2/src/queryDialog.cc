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
	if (m_name.empty() == true)
	{
		queryOkbutton->set_sensitive(false);
	}
	else
	{
		nameEntry->set_text(m_name);
	}
	// Query text
	RefPtr<TextBuffer> refBuffer = queryTextview->get_buffer();
	if (refBuffer)
	{
		refBuffer->set_text(m_properties.getFreeQuery());
	}
	// Maximum number of results
	resultsCountSpinbutton->set_value((double)m_properties.getMaximumResultsCount());
	// Index all results
	indexCheckbutton->set_active(m_properties.getIndexResults());

	// Populate
	populate_comboboxes();
}

queryDialog::~queryDialog()
{
}

bool queryDialog::is_separator(const RefPtr<TreeModel>& model, const TreeModel::iterator& iter)
{
	if (iter)
	{
		const TreeModel::Path queryPath = model->get_path(iter);
		string rowPath(from_utf8(queryPath.to_string()));
		unsigned int rowPos = 0;

		// FIXME: this is extremely hacky !
		if ((sscanf(rowPath.c_str(), "%u", &rowPos) == 1) &&
			(rowPos == 9))
		{
			return true;
		}
	}

	return false;
}

void queryDialog::populate_comboboxes()
{
	unsigned int labelNum = 1;

	// All supported filters
	filterCombobox->set_row_separator_func(SigC::slot(*this, &queryDialog::is_separator));
	filterCombobox->append_text(_("Host name"));
	filterCombobox->append_text(_("File name"));
	filterCombobox->append_text(_("File extension"));
	filterCombobox->append_text(_("Title"));
	filterCombobox->append_text(_("URL"));
	filterCombobox->append_text(_("Directory"));
	filterCombobox->append_text(_("Language code"));
	filterCombobox->append_text(_("MIME type"));
	filterCombobox->append_text(_("Label"));
	// And separate numerical ranges
	filterCombobox->append_text("===");
	filterCombobox->append_text(_("Date"));
	filterCombobox->append_text(_("Time"));
	filterCombobox->append_text(_("Size"));
	filterCombobox->set_active(0);

	// Sort order
	sortOrderCombobox->append_text(_("By relevance"));
	sortOrderCombobox->append_text(_("By date"));
	if (m_properties.getSortOrder() == QueryProperties::DATE)
	{
		sortOrderCombobox->set_active(1);
	}
	else
	{
		sortOrderCombobox->set_active(0);
	}

	// Labels
	labelNameCombobox->append_text(_("None"));
	labelNameCombobox->set_active(0);
	// Add all labels to the label combo and select the one defined for the query
	for (set<string>::const_iterator labelIter = m_labels.begin(); labelIter != m_labels.end(); ++labelIter)
	{
		string labelName(*labelIter);

		labelNameCombobox->append_text(to_utf8(labelName));
		if (labelName == m_properties.getLabelName())
		{
			labelNameCombobox->set_active(labelNum);
		}

		++labelNum;
	}
}

bool queryDialog::badName(void) const
{
	return m_badName;
}

void queryDialog::on_queryOkbutton_clicked()
{
	ustring newName(nameEntry->get_text());

	// Name
	m_properties.setName(newName);
	m_badName = false;
	// Did the name change ?
	if (m_name != newName)
	{
		const std::map<string, QueryProperties> &queries = PinotSettings::getInstance().getQueries();

		// Is it already used ?
		std::map<string, QueryProperties>::const_iterator queryIter = queries.find(newName);
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
	if (refBuffer)
	{
		m_properties.setFreeQuery(refBuffer->get_text());
	}
	// Maximum number of results
	m_properties.setMaximumResultsCount((unsigned int)resultsCountSpinbutton->get_value());
	// Sort order
	if (sortOrderCombobox->get_active_row_number() == 1)
	{
		m_properties.setSortOrder(QueryProperties::DATE);
	}
	else
	{
		m_properties.setSortOrder(QueryProperties::RELEVANCE);
	}
	// Index all results
	m_properties.setIndexResults(indexCheckbutton->get_active());
	// Index label
	m_properties.setLabelName("");
	int chosenLabel = labelNameCombobox->get_active_row_number();
	if (chosenLabel > 0)
	{
		m_properties.setLabelName(from_utf8(labelNameCombobox->get_active_text()));
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
	time_t timeT = time(NULL);
	struct tm *tm = localtime(&timeT);
	bool hasValue = true;

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
		case 9:
			// Separator
			break;
		case 10:
			filter = TimeConverter::toYYYYMMDDString(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
			filter += "..20991231";
			hasValue = false;
			break;
		case 11:
			filter = TimeConverter::toHHMMSSString(tm->tm_hour, tm->tm_min, tm->tm_sec);
			filter += "..235959";
			hasValue = false;
			break;
		case 12:
			filter += "0..10240b";
			hasValue = false;
			break;
		default:
			break;
	}

	RefPtr<TextBuffer> refBuffer = queryTextview->get_buffer();
	if (refBuffer)
	{
		ustring queryText = refBuffer->get_text();
		queryText += " ";
		queryText += filter;
		if (hasValue == true)
		{
			queryText += ":xxx";
		}
		refBuffer->set_text(queryText);
	}
}

