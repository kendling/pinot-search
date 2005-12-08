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

#ifndef _ACTION_HISTORY_H
#define _ACTION_HISTORY_H

#include <string>

#include "SQLiteBase.h"

using namespace std;

class ActionHistory : public SQLiteBase
{
	public:
		ActionHistory(const string &database);
		virtual ~ActionHistory();

		/// Creates the ActionHistory table in the database.
		static bool create(const string &database);

		typedef enum { ACTION_INDEX = 0, ACTION_UPDATE, ACTION_UNINDEX } ActionType;

		/// Inserts an item.
		bool insertItem(ActionType type, const string &option);

		/// Gets and deletes the oldest item.
		bool deleteOldestItem(ActionType &type, string &option);

	protected:
		bool getOldestItem(ActionType &type, string &date, string &option) const;

	private:
		ActionHistory(const ActionHistory &other);
		ActionHistory &operator=(const ActionHistory &other);

};

#endif // _ACTION_HISTORY_H
