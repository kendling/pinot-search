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
 
#ifndef _MONITORHANDLER_HH
#define _MONITORHANDLER_HH

#include <time.h>
#include <string>
#include <set>
#include <map>
#include <sigc++/slot.h>

#include "DocumentInfo.h"
#include "IndexedDocument.h"
#include "MboxParser.h"
#include "LabelManager.h"
#include "IndexInterface.h"
#include "PinotSettings.h"

class MonitorHandler
{
	public:
		MonitorHandler();
		virtual ~MonitorHandler();

		/// Returns a map of locations to monitor.
		virtual unsigned int getFileSystemLocations(std::map<unsigned long, std::string> &fsLocations) = 0;

		/// Returns true if the locations map has changed since last time.
		virtual bool hasNewLocations(void) const = 0;

		/// Handles file exists events.
		virtual bool fileExists(const std::string &fileName, bool end = false) = 0;

		/// Handles file creation events.
		virtual void fileCreated(const std::string &fileName) = 0;

		/// Handles file changed events.
		virtual bool fileChanged(const std::string &fileName) = 0;

		/// Handles file deleted events.
		virtual bool fileDeleted(const std::string &fileName) = 0;

		SigC::Signal3<void, IndexedDocument, unsigned int, std::string>& getUpdateSignal(void);

	protected:
		SigC::Signal3<void, IndexedDocument, unsigned int, std::string> m_signalUpdate;

	private:
		MonitorHandler(const MonitorHandler &other);
		MonitorHandler &operator=(const MonitorHandler &other);

};

class MboxHandler : public MonitorHandler
{
	public:
		MboxHandler();
		virtual ~MboxHandler();

		virtual unsigned int getFileSystemLocations(std::map<unsigned long, std::string> &fsLocations);

		virtual bool hasNewLocations(void) const;

		virtual bool fileExists(const std::string &fileName, bool end = false);

		virtual void fileCreated(const std::string &fileName);

		virtual bool fileChanged(const std::string &fileName);

		virtual bool fileDeleted(const std::string &fileName);

	protected:
		unsigned int m_locationsCount;
		bool m_hasNewLocations;

		bool checkMailAccount(const std::string &fileName, PinotSettings::MailAccount &mailAccount,
			off_t &previousSize);

		bool parseMailAccount(MboxParser &boxParser, IndexInterface *pIndex,
			LabelManager &labelMan, time_t &lastMessageTime,
			const std::string &tempSourceLabel, const std::string &sourceLabel);

		bool deleteMessages(IndexInterface *pIndex, LabelManager &labelMan,
			const std::string &sourceLabel);

	private:
		MboxHandler(const MboxHandler &other);
		MboxHandler &operator=(const MboxHandler &other);

};

#endif	// _MONITORHANDLER_HH
