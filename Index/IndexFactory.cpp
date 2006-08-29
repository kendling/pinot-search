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

#include "XapianIndex.h"
#include "DBusXapianIndex.h"
#include "IndexFactory.h"

using std::string;

IndexFactory::IndexFactory()
{
}

IndexFactory::~IndexFactory()
{
}

/// Returns an index of the specified type; NULL if unavailable.
IndexInterface *IndexFactory::getIndex(const string &type, const string &option)
{
	IndexInterface *pIndex = NULL;

	// Choice by type
	if (type == "dbus")
	{
		pIndex = new DBusXapianIndex(option);
	}
	else if (type == "xapian")
	{
		pIndex = new XapianIndex(option);
	}

	return pIndex;
}

