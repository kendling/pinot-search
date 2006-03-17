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

#ifndef _VIEW_HISTORY_H
#define _VIEW_HISTORY_H

#include <time.h>
#include <string>

#include "SQLiteBase.h"

using namespace std;

class ViewHistory : public SQLiteBase
{
	public:
		ViewHistory(const string &database);
		virtual ~ViewHistory();

		/// Creates the ViewHistory table in the database.
		static bool create(const string &database);

		/// Inserts an URL.
		bool insertItem(const string &url);

		/// Checks if an URL is in the history.
		bool hasItem(const string &url) const;

		/// Deletes an URL.
		bool deleteItem(const string &url);

		/// Expires items older than the given date.
		bool expireItems(time_t expiryDate);

	private:
		ViewHistory(const ViewHistory &other);
		ViewHistory &operator=(const ViewHistory &other);

};

#endif // _VIEW_HISTORY_H
