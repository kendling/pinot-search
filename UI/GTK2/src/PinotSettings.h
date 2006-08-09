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
#include <vector>
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

		void clear(void);

		bool loadGlobal(const std::string &fileName);

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
		std::map<std::string, bool> &getSearchEnginesChannels(void);

		/// Returns the queries map, keyed by name.
		const std::map<std::string, QueryProperties> &getQueries(void) const;

		/// Adds a new query.
		bool addQuery(const QueryProperties &properties);

		/// Removes a query.
		bool removeQuery(const std::string &name);

		/// Clears the queries map.
		void clearQueries(void);

		/// Returns the labels list.
		const std::set<std::string> &getLabels(void) const;

		/// Adds a new label.
		void addLabel(const std::string &name);

		/// Removes a label.
		void removeLabel(const std::string &name);

		/// Clears the labels list.
		void clearLabels(void);

		class TimestampedItem
		{
			public:
				TimestampedItem();
				TimestampedItem(const TimestampedItem &other);
				~TimestampedItem();

				TimestampedItem &operator=(const TimestampedItem &other);
				bool operator<(const TimestampedItem &other) const;
				bool operator==(const TimestampedItem &other) const;

				Glib::ustring m_name;
				time_t m_modTime;
		};

		class CacheProvider
		{
			public:
				CacheProvider();
				CacheProvider(const CacheProvider &other);
				~CacheProvider();

				CacheProvider &operator=(const CacheProvider &other);
				bool operator<(const CacheProvider &other) const;
				bool operator==(const CacheProvider &other) const;

				Glib::ustring m_name;
				Glib::ustring m_location;
				std::set<Glib::ustring> m_protocols;
		};

		Glib::ustring m_googleAPIKey;
		Glib::ustring m_docsIndexLocation;
		Glib::ustring m_mailIndexLocation;
		Glib::ustring m_daemonIndexLocation;
		Glib::ustring m_historyDatabase;
		int m_xPos;
		int m_yPos;
		int m_width;
		int m_height;
		int m_panePos;
		bool m_expandQueries;
		bool m_ignoreRobotsDirectives;
		bool m_suggestQueryTerms;
		unsigned short m_newResultsColourRed;
		unsigned short m_newResultsColourGreen;
		unsigned short m_newResultsColourBlue;
		std::set<TimestampedItem> m_mailAccounts;
		std::set<TimestampedItem> m_indexableLocations;
		std::vector<CacheProvider> m_cacheProviders;
		std::set<Glib::ustring> m_cacheProtocols;

	protected:
		static PinotSettings m_instance;
		bool m_firstRun;
		std::map<std::string, std::string> m_indexNames;
		std::map<unsigned int, std::string> m_indexIds;
		std::set<Engine> m_engines;
		std::map<unsigned int, std::string> m_engineIds;
		std::map<std::string, bool> m_engineChannels;
		std::map<std::string, QueryProperties> m_queries;
		std::set<std::string> m_labels;

		PinotSettings();
		bool loadConfiguration(const std::string &fileName, bool isGlobal);
		bool loadUi(const xmlpp::Element *pElem);
		bool loadIndexes(const xmlpp::Element *pElem);
		bool loadEngineChannels(const xmlpp::Element *pElem);
		bool loadQueries(const xmlpp::Element *pElem);
		bool loadLabels(const xmlpp::Element *pElem);
		bool loadColour(const xmlpp::Element *pElem);
		bool loadMailAccounts(const xmlpp::Element *pElem);
		bool loadIndexableLocations(const xmlpp::Element *pElem);
		bool loadCacheProviders(const xmlpp::Element *pElem);

	private:
		PinotSettings(const PinotSettings &other);
		PinotSettings &operator=(const PinotSettings &other);

};

#endif // _PINOTSETTINGS_HH
