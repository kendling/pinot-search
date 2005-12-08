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

#ifndef _PINOTSETTINGS_HH
#define _PINOTSETTINGS_HH

#include <sys/types.h>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <glibmm/ustring.h>
#include <libxml++/nodes/element.h>

#include "QueryProperties.h"

class PinotSettings
{
	public:
		~PinotSettings();

		static PinotSettings &getInstance(void);

		static std::string getConfigurationDirectory(void);

		static std::string getConfigurationFileName(void);

		static std::string getCurrentUserName(void);

		bool load(void);

		bool loadSearchEngines(const std::string &directoryName);

		bool save(void);

		/// Returns the indexes map, keyed by name.
		const std::map<std::string, std::string> &getIndexes(void) const;

		/// Returns true if the given index is internal.
		bool isInternalIndex(const string &indexName) const;

		/// Adds a new index.
		bool addIndex(const std::string &name, const std::string &location);

		/// Removes an index.
		bool removeIndex(const std::string &name);

		/// Clears the indexes map.
		void clearIndexes(void);

		/// Returns an ID that identifies the given index.
		unsigned int getIndexId(const std::string &name);

		/// Returns the name(s) for the given ID.
		void getIndexNames(unsigned int id, std::set<std::string> &names);

		class Engine
		{
			public:
				Engine();
				Engine(std::string name, std::string type, std::string option, std::string channel);
				~Engine();

				bool operator<(const Engine &other) const;
				bool operator==(const Engine &other) const;

				std::string m_name;
				std::string m_type;
				std::string m_option;
				std::string m_channel;
		};

		/// Returns the search engines set.
		bool getSearchEngines(std::set<Engine> &engines, std::string channelName = "") const;

		/// Returns an ID that identifies the given engine name.
		unsigned int getEngineId(const std::string &name);

		/// Returns the name(s) for the given ID.
		void getEngineNames(unsigned int id, std::set<std::string> &names);

		/// Returns the search engines channels.
		const std::set<std::string> &getSearchEnginesChannels(void) const;

		/// Returns the queries map, keyed by name.
		const std::map<std::string, QueryProperties> &getQueries(void) const;

		/// Adds a new query.
		bool addQuery(const QueryProperties &properties);

		/// Removes a query.
		bool removeQuery(const std::string &name);

		/// Clears the queries map.
		void clearQueries(void);

		class Label
		{
			public:
				Label();
				Label(Glib::ustring &name, unsigned short red,
					unsigned short green, unsigned short blue);
				~Label();

				bool operator<(const Label &other) const;
				bool operator==(const Label &other) const;

				Glib::ustring m_name;
				unsigned short m_red;
				unsigned short m_green;
				unsigned short m_blue;
		};

		class MailAccount
		{
			public:
				MailAccount();
				MailAccount(const MailAccount &other);
				~MailAccount();

				bool operator<(const MailAccount &other) const;
				bool operator==(const MailAccount &other) const;

				Glib::ustring m_name;
				Glib::ustring m_type;
				time_t m_modTime;
				time_t m_lastMessageTime;
				off_t m_size;
		};

		Glib::ustring m_googleAPIKey;
		Glib::ustring m_indexLocation;
		Glib::ustring m_mailIndexLocation;
		Glib::ustring m_historyDatabase;
		bool m_browseResults;
		Glib::ustring m_browserCommand;
		int m_xPos;
		int m_yPos;
		int m_width;
		int m_height;
		int m_panePos;
		std::set<Label> m_labels;
		bool m_ignoreRobotsDirectives;
		std::set<MailAccount> m_mailAccounts;

	protected:
		static PinotSettings m_instance;
		bool m_firstRun;
		std::map<std::string, std::string> m_indexNames;
		std::map<unsigned int, std::string> m_indexIds;
		std::set<Engine> m_engines;
		std::map<unsigned int, std::string> m_engineIds;
		std::set<std::string> m_engineChannels;
		std::map<std::string, QueryProperties> m_queries;

		PinotSettings();
		bool loadConfiguration(const std::string &fileName);
		bool loadUi(const xmlpp::Element *pElem);
		bool loadIndexes(const xmlpp::Element *pElem);
		bool loadQueries(const xmlpp::Element *pElem);
		bool loadResults(const xmlpp::Element *pElem);
		bool loadLabels(const xmlpp::Element *pElem);
		bool loadMailAccounts(const xmlpp::Element *pElem);

	private:
		PinotSettings(const PinotSettings &other);
		PinotSettings &operator=(const PinotSettings &other);

};

#endif // _PINOTSETTINGS_HH
