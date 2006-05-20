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
 
#ifndef _MBOXHANDLER_HH
#define _MBOXHANDLER_HH

#include <time.h>
#include <string>
#include <set>
#include <map>
#include <sigc++/slot.h>

#include "IndexedDocument.h"
#include "MboxParser.h"
#include "IndexInterface.h"
#include "MonitorHandler.h"
#include "PinotSettings.h"

class MboxHandler : public MonitorHandler
{
	public:
		MboxHandler();
		virtual ~MboxHandler();

		/// Returns locations.
		virtual bool getLocations(std::set<std::string> &newLocations,
			std::set<std::string> &locationsToRemove);

		/// Handles file existence events.
		virtual bool fileExists(const std::string &fileName);

		/// Handles file creation events.
		virtual bool fileCreated(const std::string &fileName);

		/// Handles file modified events.
		virtual bool fileModified(const std::string &fileName);

		/// Handles file moved events.
		virtual bool fileMoved(const std::string &fileName);

		/// Handles file deleted events.
		virtual bool fileDeleted(const std::string &fileName);

	protected:
		unsigned int m_locationsCount;
		std::set<std::string> m_locations;

		bool checkMailAccount(const std::string &fileName, PinotSettings::MailAccount &mailAccount,
			off_t &previousSize);

		bool parseMailAccount(MboxParser &boxParser, IndexInterface *pIndex,
			time_t &lastMessageTime, const std::string &tempSourceLabel,
			const std::string &sourceLabel);

		bool deleteMessages(IndexInterface *pIndex, const std::string &sourceLabel);

	private:
		MboxHandler(const MboxHandler &other);
		MboxHandler &operator=(const MboxHandler &other);

};

#endif // _MBOXHANDLER_HH
