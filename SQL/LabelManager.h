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

#ifndef _LABEL_MANAGER_H
#define _LABEL_MANAGER_H

#include <string>
#include <map>
#include <set>

#include "DocumentInfo.h"
#include "SQLiteBase.h"

using namespace std;

class LabelManager : public SQLiteBase
{
	public:
		LabelManager(const string &database);
		virtual ~LabelManager();

		/// Creates the necessary tables in the database.
		static bool create(const string &database);

		/// Gets a list of items with the given label.
		bool getDocumentsWithLabel(const string &labelName, const string &sourceName,
			set<unsigned int> &items) const;

		/// Checks if a document has a label.
		bool hasLabel(unsigned int docId, const string &sourceName,
			const string &labelName) const;

		/// Sets a document's labels.
		bool setLabels(unsigned int docId, const string &sourceName,
			const set<string> &labels);

		/// Gets the labels for the given document.
		bool getLabels(unsigned int docId, const string &sourceName,
			set<string> &labels) const;

		/// Renames a label.
		bool renameLabel(const string &name, const string &newName);

		/// Deletes all references to a label.
		bool deleteLabel(const string &name);

		/// Deletes an item.
		bool deleteItem(unsigned int docId, const string &sourceName);

	protected:
		unsigned int getLabelId(const string &labelName) const;

		unsigned int getNewLabelId(void) const;

	private:
		LabelManager(const LabelManager &other);
		LabelManager &operator=(const LabelManager &other);

};

#endif // _LABEL_MANAGER_H
