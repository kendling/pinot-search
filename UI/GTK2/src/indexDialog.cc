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

#include "config.h"
#include "NLS.h"
#include "PinotSettings.h"
#include "PinotUtils.h"
#include "indexDialog.hh"

using namespace std;
using namespace Glib;
using namespace Gtk;

indexDialog::indexDialog() :
	indexDialog_glade(),
	m_editIndex(false),
	m_badName(true)
{
	// Associate the columns model to the type combo
	m_refTypeTree = ListStore::create(m_typeColumns);
	typeCombobox->set_model(m_refTypeTree);
	typeCombobox->pack_start(m_typeColumns.m_name);
	// Populate
	populate_typeCombobox();

	// By default, type is set to local
	typeCombobox->set_active(0);
	portSpinbutton->set_sensitive(false);
	// ...and both name and location are empty
	indexOkbutton->set_sensitive(false);
}

indexDialog::indexDialog(const ustring &name, const ustring &location) :
	indexDialog_glade(),
	m_name(name),
	m_location(location),
	m_editIndex(false),
	m_badName(true)
{
	ustring dirName = location;
	unsigned int port = 1024;

	// Associate the columns model to the type combo
	m_refTypeTree = ListStore::create(m_typeColumns);
	typeCombobox->set_model(m_refTypeTree);
	typeCombobox->pack_start(m_typeColumns.m_name);
	// Populate
	populate_typeCombobox();

	// Name
	nameEntry->set_text(name);

	// Type and location
	ustring::size_type colonPos = location.find(":");
	if ((location[0] != '/') &&
		(colonPos != ustring::npos))
	{
		// This is a remote index
		dirName = location.substr(0, colonPos);
		port = (unsigned int)atoi(location.substr(colonPos + 1).c_str());

		typeCombobox->set_active(1);
		locationButton->set_sensitive(false);

		// Port
		portSpinbutton->set_value((double)port);
	}
	else
	{
		typeCombobox->set_active(0);
		portSpinbutton->set_sensitive(false);
	}
	locationEntry->set_text(dirName);

	m_editIndex = true;
}

indexDialog::~indexDialog()
{
}

void indexDialog::populate_typeCombobox(void)
{
	TreeModel::iterator iter = m_refTypeTree->append();
	TreeModel::Row row = *iter;
	row[m_typeColumns.m_name] = "Local";
	iter = m_refTypeTree->append();
	row = *iter;
	row[m_typeColumns.m_name] = "Remote";
}

void indexDialog::checkFields(void)
{
	bool isLocal = false, enableOkButton = false;

	if (typeCombobox->get_active_row_number() == 0)
	{
		// Local index
		portSpinbutton->set_sensitive(false);
		locationButton->set_sensitive(true);
		isLocal = true;
	}
	else
	{
		// Remote index
		portSpinbutton->set_sensitive(true);
		locationButton->set_sensitive(false);
	}

	ustring location = locationEntry->get_text();
	if (location.empty() == false)
	{
		bool startsWithSlash = false;

		if (location[0] == '/')
		{
			startsWithSlash = true;
		}

		// Disable the OK button if the type+location pair doesn't make sense
		// or if name is empty
		ustring name = nameEntry->get_text();
		if ((name.empty() == false) &&
			(startsWithSlash == isLocal))
		{
			enableOkButton = true;
		}
	}

	indexOkbutton->set_sensitive(enableOkButton);
}

bool indexDialog::badName(void) const
{
	return m_badName;
}

ustring indexDialog::getName(void) const
{
	return m_name;
}

ustring indexDialog::getLocation(void) const
{
	return m_location;
}

void indexDialog::on_indexOkbutton_clicked()
{
	PinotSettings &settings = PinotSettings::getInstance();

	// The changed() methods ensure name and location are set
	ustring name = nameEntry->get_text();
	ustring location = locationEntry->get_text();
	m_badName = false;

	// Is it a remote index ?
	if (typeCombobox->get_active_row_number() == 1)
	{
		char portStr[64];
		int port = portSpinbutton->get_value_as_int();
		snprintf(portStr, 64, "%d", port);

		// Append the port number
		location += ":";
		location += portStr;
	}
#ifdef DEBUG
	cout << "indexDialog::on_indexOkbutton_clicked: " << name << ", " << location << endl;
#endif

	// Look up that index name in the map
	const std::map<string, string> &indexesMap = settings.getIndexes();
	std::map<string, string>::const_iterator indexIter = indexesMap.find(from_utf8(name));
	if (indexIter != indexesMap.end())
	{
		// This name is in use
		m_badName = true;
#ifdef DEBUG
		cout << "indexDialog::on_indexOkbutton_clicked: name in use" << endl;
#endif
	}

	if ((m_editIndex == true) &&
		(name == m_name))
	{
		// ... but that's okay, because it's the original name
		m_badName = false;
#ifdef DEBUG
		cout << "indexDialog::on_indexOkbutton_clicked: old name" << endl;
#endif
	}

	m_name = name;
	m_location = location;
}

void indexDialog::on_locationEntry_changed()
{
	checkFields();
}

void indexDialog::on_locationButton_clicked()
{
	ustring dirName = locationEntry->get_text();
	if (select_file_name(*this, _("Index location"), dirName, true, true) == true)
	{
		locationEntry->set_text(dirName);
	}
}

void indexDialog::on_typeCombobox_changed()
{
	checkFields();

}

void indexDialog::on_nameEntry_changed()
{
	checkFields();
}
