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

#include "Languages.h"

using std::string;
using std::map;
using std::pair;

unsigned int Languages::m_count = 12;

char *Languages::m_names[] = {"Danish", "Dutch", "English", "Finnish", \
	"French", "German", "Italian", "Norwegian", "Portuguese", "Russian", \
	"Spanish", "Swedish" };

char *Languages::m_codes[] = { "da", "nl", "en", "fi", "fr", "de", \
	"it", "nn", "pt", "ru", "es", "sv" };

map<unsigned int, string> Languages::m_intlNames;

Languages::Languages()
{
}

Languages::~Languages()
{
}

bool Languages::setIntlName(unsigned int num, const string &name)
{
		pair<map<unsigned int, string>::iterator, bool> insertPair = m_intlNames.insert(pair<unsigned int, string>(num, name));
		// Was it inserted ?
		return insertPair.second;
}

string Languages::getIntlName(unsigned int num)
{
	map<unsigned int, string>::iterator iter = m_intlNames.find(num);
	if (iter == m_intlNames.end())
	{
		return "";
	}

	return iter->second;
}
