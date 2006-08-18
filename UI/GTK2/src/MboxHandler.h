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
#include <sigc++/slot.h>

#include "IndexedDocument.h"
#include "MboxParser.h"
#include "CrawlHistory.h"
#include "WritableXapianIndex.h"
#include "MonitorHandler.h"
#include "PinotSettings.h"

class MboxHandler : public MonitorHandler
{
	public:
		MboxHandler();
		virtual ~MboxHandler();

		/// Initializes things before starting monitoring.
		virtual void initialize(void);

		/// Handles flushing the index.
		virtual void flushIndex(void);

		/// Handles file existence events.
		virtual bool fileExists(const std::string &fileName);

		/// Handles file creation events.
		virtual bool fileCreated(const std::string &fileName);

		/// Handles file modified events.
		virtual bool fileModified(const std::string &fileName);

		/// Handles file moved events.
		virtual bool fileMoved(const std::string &fileName,
			const std::string &previousFileName);

		/// Handles file deleted events.
		virtual bool fileDeleted(const std::string &fileName);

	protected:
		CrawlHistory m_history;
		WritableXapianIndex m_index;
		unsigned int m_sourceId;

		bool checkMailAccount(const std::string &fileName, PinotSettings::TimestampedItem &mailAccount);

		bool indexMessages(const std::string &fileName, PinotSettings::TimestampedItem &mailAccount,
			off_t mboxOffset);

		bool parseMailAccount(MboxParser &boxParser, const std::string &sourceLabel);

		bool deleteMessages(std::set<unsigned int> &docIdList);

	private:
		MboxHandler(const MboxHandler &other);
		MboxHandler &operator=(const MboxHandler &other);

};

#endif // _MBOXHANDLER_HH
