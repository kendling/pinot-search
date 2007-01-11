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
#include <utility>
#include <string>
#include <glibmm/ustring.h>
#include <sigc++/slot.h>

#include "config.h"
#include "NLS.h"
#include "PinotSettings.h"
#include "PinotUtils.h"
#include "dateDialog.hh"

using namespace std;
using namespace Glib;
using namespace Gtk;

dateDialog::dateDialog(const ustring &title, const Date &date) :
	dateDialog_glade()
{
	int month = (int )date.get_month();

	set_title(title);

	dateCalendar->select_month((guint )max(--month, 0), (guint )date.get_year());
	dateCalendar->select_day((guint )date.get_day());
}

dateDialog::~dateDialog()
{
}

void dateDialog::getChoice(Date &chosenDate) const
{
	Date dateNow;
	guint year, month, day;

	dateNow.set_time_current();
	dateCalendar->get_date(year, month, day);
#ifdef DEBUG
	cout << "dateDialog::getChoice: selected " << year << " " << month << " " << day << endl;
#endif
	chosenDate.set_dmy((Date::Day )day, (Date::Month )++month, (Date::Year )year);
}

