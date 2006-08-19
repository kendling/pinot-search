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
#include "WritableXapianIndex.h"
#include "IndexFactory.h"

using std::string;

IndexFactory::IndexFactory()
{
}

IndexFactory::~IndexFactory()
{
}

/// Returns a read-only index of the specified type; NULL if unavailable.
IndexInterface *IndexFactory::getROIndex(const string &type, const string &option)
{
	// Whatever the type, all read-only operations can be performed with XapianIndex
	return new XapianIndex(option);
}

/// Returns a read-write index of the specified type; NULL if unavailable.
WritableIndexInterface *IndexFactory::getRWIndex(const string &type, const string &option)
{
	WritableIndexInterface *pIndex = NULL;

	// Choice by type
	if (type == "dbus")
	{
		pIndex = new DBusXapianIndex(option);
	}
	else if (type == "xapian")
	{
		pIndex = new WritableXapianIndex(option);
	}

	return pIndex;
}

