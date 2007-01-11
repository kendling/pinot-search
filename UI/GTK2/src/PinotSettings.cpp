/*
 *  Copyright 2005,2006 Fabrice Colin
 *
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <fnmatch.h>
#include <algorithm>
#include <iostream>

#include <glibmm/convert.h>
#include <glibmm/date.h>
#include <libxml++/parsers/domparser.h>
#include <libxml++/nodes/node.h>
#include <libxml++/nodes/textnode.h>

#include "config.h"
#include "Languages.h"
#include "NLS.h"
#include "StringManip.h"
#include "IndexFactory.h"
#include "PluginWebEngine.h"
#include "PinotSettings.h"

using namespace std;
using namespace Glib;
using namespace xmlpp;

static string getElementContent(const Element *pElem)
{
	if (pElem == NULL)
	{
		return "";
	}

#ifdef HAS_LIBXMLPP026
	const TextNode *pText = pElem->get_child_content();
#else
	const TextNode *pText = pElem->get_child_text();
#endif
	if (pText == NULL)
	{
		return "";
	}

	return pText->get_content();
}

static Element *addChildElement(Element *pElem, const string &nodeName, const string &nodeContent)
{
	if (pElem == NULL)
	{
		return NULL;
	}

	Element *pSubElem = pElem->add_child(nodeName);
	if (pSubElem != NULL)
	{
#ifdef HAS_LIBXMLPP026
		pSubElem->set_child_content(nodeContent);
#else
		pSubElem->set_child_text(nodeContent);
#endif
	}

	return pSubElem;
}

PinotSettings PinotSettings::m_instance;
bool PinotSettings::m_enableDBus = false;

PinotSettings::PinotSettings() :
	m_xPos(0),
	m_yPos(0),
	m_width(0),
	m_height(0),
	m_panePos(-1),
	m_showEngines(false),
	m_expandQueries(false),
	m_ignoreRobotsDirectives(false),
	m_suggestQueryTerms(true),
	m_newResultsColourRed(65535),
	m_newResultsColourGreen(0),
	m_newResultsColourBlue(0),
	m_firstRun(false)
{
	// Find out if there is a .pinot directory
	struct stat fileStat;
	string directoryName = getConfigurationDirectory();
	if (stat(directoryName.c_str(), &fileStat) != 0)
	{
		// No, create it then
		if (mkdir(directoryName.c_str(), (mode_t)S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IXOTH) == 0)
		{
			m_firstRun = true;
		}
		else
		{
			cerr << "Couldn't create pinot directory at "
				<< directoryName << endl;
		}
	}

	// This is where the internal indices live
	m_docsIndexLocation = directoryName;
	m_docsIndexLocation += "/index";
	m_daemonIndexLocation = directoryName;
	m_daemonIndexLocation += "/daemon";

	// The location of the history database is not configurable
	m_historyDatabase = directoryName;
	m_historyDatabase += "/history";
}

PinotSettings::~PinotSettings()
{
}

PinotSettings &PinotSettings::getInstance(void)
{
	return m_instance;
}

bool PinotSettings::enableDBus(bool enable)
{
	bool isEnabled = m_enableDBus;

	m_enableDBus = enable;

	return isEnabled;
}

string PinotSettings::getConfigurationDirectory(void)
{
	struct passwd *pPasswd = getpwuid(geteuid());
	string directoryName;

	if ((pPasswd != NULL) &&
		(pPasswd->pw_dir != NULL))
	{
		directoryName = pPasswd->pw_dir;
	}
	else
	{
		char *homeDir = getenv("HOME");
		if (homeDir != NULL)
		{
			directoryName = homeDir;
		}
		else
		{
			return "~/.pinot";
		}
	}
	directoryName += "/.pinot";

	return directoryName;
}

string PinotSettings::getConfigurationFileName(void)
{
	string configFileName = getConfigurationDirectory();
	configFileName += "/config.xml";

	return configFileName;
}

string PinotSettings::getCurrentUserName(void)
{
	struct passwd *pPasswd = getpwuid(geteuid());

	if ((pPasswd != NULL) &&
		(pPasswd->pw_name != NULL))
	{
		return pPasswd->pw_name;
	}

	return "";
}

bool PinotSettings::isFirstRun(void) const
{
	return m_firstRun;
}

void PinotSettings::clear(void)
{
	// Clear lists
	m_indexNames.clear();
	m_indexIds.clear();
	m_engines.clear();
	m_engineIds.clear();
	m_queries.clear();
	m_labels.clear();
	m_mailAccounts.clear();
	m_indexableLocations.clear();
	m_filePatternsBlackList.clear();
	m_cacheProviders.clear();
}

bool PinotSettings::loadGlobal(const string &fileName)
{
	if (loadConfiguration(fileName, true) == false)
	{
		return false;
	}

	return true;
}


bool PinotSettings::load(void)
{
	// Load the configuration file
	string fileName = getConfigurationFileName();
	if (m_firstRun == false)
	{
		loadConfiguration(fileName, false);
	}

	// Internal indices
	addIndex(_("My Web Pages"), m_docsIndexLocation);
	addIndex(_("My Documents"), m_daemonIndexLocation);

	if (m_firstRun == true)
	{
		// Add default labels
		m_labels.insert(_("Important"));
		m_labels.insert(_("New"));
		m_labels.insert(_("Personal"));
		// Skip common image, video and archive types
		m_filePatternsBlackList.insert("*.Z");
		m_filePatternsBlackList.insert("*.avi");
		m_filePatternsBlackList.insert("*.asf");
		m_filePatternsBlackList.insert("*.bz2");
		m_filePatternsBlackList.insert("*.deb");
		m_filePatternsBlackList.insert("*.gif");
		m_filePatternsBlackList.insert("*.gz");
		m_filePatternsBlackList.insert("*.jpeg");
		m_filePatternsBlackList.insert("*.jpg");
		m_filePatternsBlackList.insert("*.png");
		m_filePatternsBlackList.insert("*.lha");
		m_filePatternsBlackList.insert("*.mov");
		m_filePatternsBlackList.insert("*.msf");
		m_filePatternsBlackList.insert("*.mpeg");
		m_filePatternsBlackList.insert("*.mpg");
		m_filePatternsBlackList.insert("*.rpm");
		m_filePatternsBlackList.insert("*.sh");
		m_filePatternsBlackList.insert("*.tar");
		m_filePatternsBlackList.insert("*.wmv");
		m_filePatternsBlackList.insert("*.zip");
	}

	// Some search engines are hardcoded
#ifdef HAVE_GOOGLEAPI
	m_engineIds[1 << m_engines.size()] = "Google API";
	m_engines.insert(Engine("Google API", "googleapi", "", "The Web"));
	m_engineChannels.insert(pair<string, bool>("The Web", true));
#endif
	m_engineIds[1 << m_engines.size()] = "Xapian";
	m_engines.insert(Engine("Xapian", "xapian", "", ""));

	return true;
}

bool PinotSettings::loadConfiguration(const std::string &fileName, bool isGlobal)
{
	struct stat fileStat;
	bool success = true;

	if ((stat(fileName.c_str(), &fileStat) != 0) ||
		(!S_ISREG(fileStat.st_mode)))
	{
		cerr << "Couldn't open configuration file " << fileName << endl;
		m_firstRun = true;
		return false;
	}

	try
	{
		// Parse the configuration file
		DomParser parser;
		parser.set_substitute_entities(true);
		parser.parse_file(fileName);
		xmlpp::Document *pDocument = parser.get_document();
		if (pDocument == NULL)
		{
			return false;
		}

		Element *pRootElem = pDocument->get_root_node();
		if (pRootElem == NULL)
		{
			return false;
		}

		// Check the top-level element is what we expect
		ustring rootNodeName = pRootElem->get_name();
		if (rootNodeName != "pinot")
		{
			return false;
		}

		// Go through the subnodes
		const Node::NodeList childNodes = pRootElem->get_children();
		if (childNodes.empty() == false)
		{
			for (Node::NodeList::const_iterator iter = childNodes.begin(); iter != childNodes.end(); ++iter)
			{
				Node *pNode = (*iter);
				// All nodes should be elements
				Element *pElem = dynamic_cast<Element*>(pNode);
				if (pElem == NULL)
				{
					continue;
				}

				string nodeName(pElem->get_name());
				string nodeContent(getElementContent(pElem));
				if (isGlobal == true)
				{
					if (nodeName == "cache")
					{
						loadCacheProviders(pElem);
					}
					else
					{
						// Unsupported element
						continue;
					}
				}
				else if (nodeName == "version")
				{
					m_version = nodeContent;
				}
				else if (nodeName == "googleapikey")
				{
					m_googleAPIKey = nodeContent;
				}
				else if (nodeName == "ui")
				{
					loadUi(pElem);
				}
				else if (nodeName == "extraindex")
				{
					loadIndexes(pElem);
				}
				else if (nodeName == "channel")
				{
					loadEngineChannels(pElem);
				}
				else if (nodeName == "storedquery")
				{
					loadQueries(pElem);
				}
				else if (nodeName == "label")
				{
					loadLabels(pElem);
				}
				else if (nodeName == "robots")
				{
					if (nodeContent == "IGNORE")
					{
						m_ignoreRobotsDirectives = true;
					}
					else
					{
						m_ignoreRobotsDirectives = false;
					}
				}
				else if (nodeName == "suggestterms")
				{
					if (nodeContent == "YES")
					{
						m_suggestQueryTerms = true;
					}
					else
					{
						m_suggestQueryTerms = false;
					}
				}
				else if (nodeName == "newresults")
				{
					loadColour(pElem);
				}
				else if (nodeName == "mailaccount")
				{
					loadMailAccounts(pElem);
				}
				else if (nodeName == "indexable")
				{
					loadIndexableLocations(pElem);
				}
				else if (nodeName == "blacklist")
				{
					loadFilePatterns(pElem);
				}
			}
		}
	}
	catch (const std::exception& ex)
	{
		cerr << "Couldn't parse configuration file: "
			<< ex.what() << endl;
		success = false;
	}

	return success;
}

bool PinotSettings::loadUi(const Element *pElem)
{
	if (pElem == NULL)
	{
		return false;
	}

	Node::NodeList childNodes = pElem->get_children();
	if (childNodes.empty() == true)
	{
		return false;
	}

	for (Node::NodeList::iterator iter = childNodes.begin(); iter != childNodes.end(); ++iter)
	{
		Node *pNode = (*iter);
		Element *pElem = dynamic_cast<Element*>(pNode);
		if (pElem == NULL)
		{
			continue;
		}

		string nodeName = pElem->get_name();
		string nodeContent = getElementContent(pElem);
		if (nodeName == "xpos")
		{
			m_xPos = atoi(nodeContent.c_str());
		}
		else if (nodeName == "ypos")
		{
			m_yPos = atoi(nodeContent.c_str());
		}
		else if (nodeName == "width")
		{
			m_width = atoi(nodeContent.c_str());
		}
		else if (nodeName == "height")
		{
			m_height = atoi(nodeContent.c_str());
		}
		else if (nodeName == "panepos")
		{
			m_panePos = atoi(nodeContent.c_str());
		}
		else if (nodeName == "expandqueries")
		{
			if (nodeContent == "YES")
			{
				m_expandQueries = true;
			}
			else
			{
				m_expandQueries = false;
			}
		}
		else if (nodeName == "showengines")
		{
			if (nodeContent == "YES")
			{
				m_showEngines = true;
			}
			else
			{
				m_showEngines = false;
			}
		}
	}

	return true;
}

bool PinotSettings::loadIndexes(const Element *pElem)
{
	if (pElem == NULL)
	{
		return false;
	}

	Node::NodeList childNodes = pElem->get_children();
	if (childNodes.empty() == true)
	{
		return false;
	}

	string indexName, indexLocation;

	for (Node::NodeList::iterator iter = childNodes.begin(); iter != childNodes.end(); ++iter)
	{
		Node *pNode = (*iter);
		Element *pElem = dynamic_cast<Element*>(pNode);
		if (pElem == NULL)
		{
			continue;
		}

		string nodeName = pElem->get_name();
		string nodeContent = getElementContent(pElem);
		if (nodeName == "name")
		{
			indexName = nodeContent;
		}
		else if (nodeName == "location")
		{
			indexLocation = nodeContent;
		}
	}

	if ((indexName.empty() == false) &&
		(indexLocation.empty() == false))
	{
		addIndex(indexName, indexLocation);
	}

	return true;
}

bool PinotSettings::loadEngineChannels(const Element *pElem)
{
	if (pElem == NULL)
	{
		return false;
	}

	Node::NodeList childNodes = pElem->get_children();
	if (childNodes.empty() == true)
	{
		return false;
	}

	for (Node::NodeList::iterator iter = childNodes.begin(); iter != childNodes.end(); ++iter)
	{
		Node *pNode = (*iter);
		Element *pElem = dynamic_cast<Element*>(pNode);
		if (pElem == NULL)
		{
			continue;
		}

		string nodeName = pElem->get_name();
		string nodeContent = getElementContent(pElem);
		if (nodeName == "name")
		{
			std::map<string, bool>::iterator channelIter = m_engineChannels.find(nodeContent);

			if (channelIter != m_engineChannels.end())
			{
				channelIter->second = false;
			}
		}
	}

	return true;
}

bool PinotSettings::loadQueries(const Element *pElem)
{
	if (pElem == NULL)
	{
		return false;
	}

	Node::NodeList childNodes = pElem->get_children();
	if (childNodes.empty() == true)
	{
		return false;
	}

	QueryProperties queryProps;
	Date minDate, maxDate;
	string backCompatString;
	bool enableMinDate = false, enableMaxDate = false;

	minDate.set_time_current();
	maxDate.set_time_current();

	// Load the query's properties
	for (Node::NodeList::iterator iter = childNodes.begin(); iter != childNodes.end(); ++iter)
	{
		Node *pNode = (*iter);
		Element *pElem = dynamic_cast<Element*>(pNode);
		if (pElem == NULL)
		{
			continue;
		}

		string nodeName = pElem->get_name();
		string nodeContent = getElementContent(pElem);
		if (nodeName == "name")
		{
			queryProps.setName(nodeContent);
		}
		else if (nodeName == "text")
		{
			queryProps.setFreeQuery(nodeContent);
		}
		else if ((nodeName == "and") &&
			(nodeContent.empty() == false))
		{
			if (backCompatString.empty() == false)
			{
				backCompatString += " ";
			}
			backCompatString += nodeContent;
		}
		else if ((nodeName == "phrase") &&
			(nodeContent.empty() == false))
		{
			if (backCompatString.empty() == false)
			{
				backCompatString += " ";
			}
			backCompatString += "\"";
			backCompatString += nodeContent;
			backCompatString += "\"";
		}
		else if ((nodeName == "any") &&
			(nodeContent.empty() == false))
		{
			// FIXME: don't be lazy and add those correctly
			if (backCompatString.empty() == false)
			{
				backCompatString += " ";
			}
			backCompatString += nodeContent;
		}
		else if ((nodeName == "not") &&
			(nodeContent.empty() == false))
		{
			if (backCompatString.empty() == false)
			{
				backCompatString += " ";
			}
			backCompatString += "-(";
			backCompatString += nodeContent;
			backCompatString += ")";
		}
		else if ((nodeName == "language") &&
			(nodeContent.empty() == false))
		{
			if (backCompatString.empty() == false)
			{
				backCompatString += " ";
			}
			backCompatString += "language:";
			backCompatString += nodeContent;
		}
		else if ((nodeName == "hostfilter") &&
			(nodeContent.empty() == false))
		{
			if (backCompatString.empty() == false)
			{
				backCompatString += " ";
			}
			backCompatString += "site:";
			backCompatString += nodeContent;
		}
		else if ((nodeName == "filefilter") &&
			(nodeContent.empty() == false))
		{
			if (backCompatString.empty() == false)
			{
				backCompatString += " ";
			}
			backCompatString += "file:";
			backCompatString += nodeContent;
		}
		else if ((nodeName == "labelfilter") &&
			(nodeContent.empty() == false))
		{
			if (backCompatString.empty() == false)
			{
				backCompatString += " ";
			}
			backCompatString += "label:";
			backCompatString += nodeContent;
		}
		else if (nodeName == "maxresults")
		{
			int count = atoi(nodeContent.c_str());
			queryProps.setMaximumResultsCount((unsigned int)max(count, 10));
		}
		else if (nodeName == "enablemindate")
		{
			if (nodeContent == "YES")
			{
				enableMinDate = true;
			}
			queryProps.setMinimumDate(enableMinDate, minDate.get_day(), minDate.get_month(), minDate.get_year());
		}
		else if (nodeName == "mindate")
		{
			minDate.set_parse(nodeContent);
			queryProps.setMinimumDate(enableMinDate, minDate.get_day(), minDate.get_month(), minDate.get_year());
		}
		else if (nodeName == "enablemaxdate")
		{
			if (nodeContent == "YES")
			{
				enableMaxDate = true;
			}
			queryProps.setMaximumDate(enableMaxDate, maxDate.get_day(), maxDate.get_month(), maxDate.get_year());
		}
		else if (nodeName == "maxdate")
		{
			maxDate.set_parse(nodeContent);
			queryProps.setMaximumDate(enableMaxDate, maxDate.get_day(), maxDate.get_month(), maxDate.get_year());
		}
		else if (nodeName == "index")
		{
			if (nodeContent == "ALL")
			{
				queryProps.setIndexResults(true);
			}
			else
			{
				queryProps.setIndexResults(false);
			}
		}
		else if (nodeName == "label")
		{
			queryProps.setLabelName(nodeContent);
		}
	}

	// We need at least a name
	if (queryProps.getName().empty() == false)
	{
		if (backCompatString.empty() == false)
		{
			queryProps.setFreeQuery(backCompatString);
		}
		m_queries[queryProps.getName()] = queryProps;
	}

	return true;
}

bool PinotSettings::loadLabels(const Element *pElem)
{
	if (pElem == NULL)
	{
		return false;
	}

	Node::NodeList childNodes = pElem->get_children();
	if (childNodes.empty() == true)
	{
		return false;
	}

	// Load the label's properties
	for (Node::NodeList::iterator iter = childNodes.begin(); iter != childNodes.end(); ++iter)
	{
		Node *pNode = (*iter);
		Element *pElem = dynamic_cast<Element*>(pNode);
		if (pElem == NULL)
		{
			continue;
		}

		string nodeName = pElem->get_name();
		string nodeContent = getElementContent(pElem);

		if (nodeName == "name")
		{
			m_labels.insert(nodeContent);
		}
		// Labels used to have colours...
	}

	return true;
}

bool PinotSettings::loadColour(const Element *pElem)
{
	if (pElem == NULL)
	{
		return false;
	}

	Node::NodeList childNodes = pElem->get_children();
	if (childNodes.empty() == true)
	{
		return false;
	}

	// Load the colour RGB components
	for (Node::NodeList::iterator iter = childNodes.begin(); iter != childNodes.end(); ++iter)
	{
		Node *pNode = (*iter);
		Element *pElem = dynamic_cast<Element*>(pNode);
		if (pElem == NULL)
		{
			continue;
		}

		string nodeName = pElem->get_name();
		string nodeContent = getElementContent(pElem);
		gushort value = (gushort)atoi(nodeContent.c_str());

		if (nodeName == "red")
		{
			m_newResultsColourRed = value;
		}
		else if (nodeName == "green")
		{
			m_newResultsColourGreen = value;
		}
		else if (nodeName == "blue")
		{
			m_newResultsColourBlue = value;
		}
	}

	return true;
}

bool PinotSettings::loadMailAccounts(const Element *pElem)
{
	if (pElem == NULL)
	{
		return false;
	}

	Node::NodeList childNodes = pElem->get_children();
	if (childNodes.empty() == true)
	{
		return false;
	}

	TimestampedItem mailAccount;

	// Load the mail account's properties
	for (Node::NodeList::iterator iter = childNodes.begin(); iter != childNodes.end(); ++iter)
	{
		Node *pNode = (*iter);
		Element *pElem = dynamic_cast<Element*>(pNode);
		if (pElem == NULL)
		{
			continue;
		}

		string nodeName = pElem->get_name();
		string nodeContent = getElementContent(pElem);

		if (nodeName == "name")
		{
			mailAccount.m_name = nodeContent;
		}
		else if (nodeName == "mtime")
		{
			mailAccount.m_modTime = (time_t)atoi(nodeContent.c_str());
		}
	}

	if (mailAccount.m_name.empty() == false)
	{
		m_mailAccounts.insert(mailAccount);
	}

	return true;
}

bool PinotSettings::loadIndexableLocations(const Element *pElem)
{
	if (pElem == NULL)
	{
		return false;
	}

	Node::NodeList childNodes = pElem->get_children();
	if (childNodes.empty() == true)
	{
		return false;
	}

	IndexableLocation location;

	// Load the indexable location's properties
	for (Node::NodeList::iterator iter = childNodes.begin(); iter != childNodes.end(); ++iter)
	{
		Node *pNode = (*iter);
		Element *pElem = dynamic_cast<Element*>(pNode);
		if (pElem == NULL)
		{
			continue;
		}

		string nodeName = pElem->get_name();
		string nodeContent = getElementContent(pElem);

		if (nodeName == "name")
		{
			location.m_name = nodeContent;
		}
		else if (nodeName == "monitor")
		{
			if (nodeContent == "YES")
			{
				location.m_monitor = true;
			}
			else
			{
				location.m_monitor = false;
			}
		}
	}

	if (location.m_name.empty() == false)
	{
		m_indexableLocations.insert(location);
	}

	return true;
}

bool PinotSettings::loadFilePatterns(const Element *pElem)
{
	if (pElem == NULL)
	{
		return false;
	}

	Node::NodeList childNodes = pElem->get_children();
	if (childNodes.empty() == true)
	{
		return false;
	}

	// Load the file patterns list
	for (Node::NodeList::iterator iter = childNodes.begin(); iter != childNodes.end(); ++iter)
	{
		Node *pNode = (*iter);
		Element *pElem = dynamic_cast<Element*>(pNode);
		if (pElem == NULL)
		{
			continue;
		}

		string nodeName = pElem->get_name();
		string nodeContent = getElementContent(pElem);

		if (nodeName == "pattern")
		{
			m_filePatternsBlackList.insert(nodeContent);
		}
	}

	return true;
}

bool PinotSettings::loadCacheProviders(const Element *pElem)
{
	if (pElem == NULL)
	{
		return false;
	}

	Node::NodeList childNodes = pElem->get_children();
	if (childNodes.empty() == true)
	{
		return false;
	}

	CacheProvider cacheProvider;

	// Load the cache provider's properties
	for (Node::NodeList::iterator iter = childNodes.begin(); iter != childNodes.end(); ++iter)
	{
		Node *pNode = (*iter);
		Element *pElem = dynamic_cast<Element*>(pNode);
		if (pElem == NULL)
		{
			continue;
		}

		string nodeName = pElem->get_name();
		string nodeContent = getElementContent(pElem);

		if (nodeName == "name")
		{
			cacheProvider.m_name = nodeContent;
		}
		else if (nodeName == "location")
		{
			cacheProvider.m_location = nodeContent;
		}
		else if (nodeName == "protocols")
		{
			nodeContent += ",";

			ustring::size_type previousPos = 0, commaPos = nodeContent.find(",");
			while (commaPos != ustring::npos)
			{
				string protocol(nodeContent.substr(previousPos,
                                        commaPos - previousPos));

				StringManip::trimSpaces(protocol);
				cacheProvider.m_protocols.insert(protocol);

				// Next
				previousPos = commaPos + 1;
				commaPos = nodeContent.find(",", previousPos);
			}
		}
	}

	if ((cacheProvider.m_name.empty() == false) &&
		(cacheProvider.m_location.empty() == false))
	{
		m_cacheProviders.push_back(cacheProvider);

		// Copy the list of protocols supported by this cache provider
		copy(cacheProvider.m_protocols.begin(), cacheProvider.m_protocols.end(),
			inserter(m_cacheProtocols, m_cacheProtocols.begin()));
	}

	return true;
}

bool PinotSettings::loadSearchEngines(const string &directoryName)
{
	if (directoryName.empty() == true)
	{
		return true;
	}

	DIR *pDir = opendir(directoryName.c_str());
	if (pDir == NULL)
	{
		return false;
	}

	// Iterate through this directory's entries
	struct dirent *pDirEntry = readdir(pDir);
	while (pDirEntry != NULL)
	{
		char *pEntryName = pDirEntry->d_name;
		if (pEntryName != NULL)
		{
			struct stat fileStat;
			string location = directoryName;
			location += "/";
			location += pEntryName;

			// Is that a file ?
			if ((stat(location.c_str(), &fileStat) == 0) &&
				(S_ISREG(fileStat.st_mode)))
			{
				string engineName, engineChannel;

				if ((PluginWebEngine::getDetails(location, engineName, engineChannel) == true) &&
					(engineName.empty() == false))
				{
					m_engineIds[1 << m_engines.size()] = engineName;
					if (engineChannel.empty() == true)
					{
						engineChannel = _("Unclassified");
					}
					m_engines.insert(Engine(engineName, "sherlock", location, engineChannel));
					m_engineChannels.insert(pair<string, bool>(engineChannel, true));
				}
			}
		}

		// Next entry
		pDirEntry = readdir(pDir);
	}
	closedir(pDir);

	return true;
}

bool PinotSettings::save(void)
{
	Element *pRootElem = NULL;
	Element *pElem = NULL;
	char numStr[64];

	xmlpp::Document doc("1.0");

	// Create a new node
	pRootElem = doc.create_root_node("pinot");
	if (pRootElem == NULL)
	{
		return false;
	}
	// ...with text children nodes
	addChildElement(pRootElem, "version", VERSION);
	addChildElement(pRootElem, "googleapikey", m_googleAPIKey);
	// User interface position and size
	pElem = pRootElem->add_child("ui");
	if (pElem == NULL)
	{
		return false;
	}
	sprintf(numStr, "%d", m_xPos);
	addChildElement(pElem, "xpos", numStr);
	sprintf(numStr, "%d", m_yPos);
	addChildElement(pElem, "ypos", numStr);
	sprintf(numStr, "%d", m_width);
	addChildElement(pElem, "width", numStr);
	sprintf(numStr, "%d", m_height);
	addChildElement(pElem, "height", numStr);
	sprintf(numStr, "%d", m_panePos);
	addChildElement(pElem, "panepos", numStr);
	addChildElement(pElem, "expandqueries", (m_expandQueries ? "YES" : "NO"));
	addChildElement(pElem, "showengines", (m_showEngines ? "YES" : "NO"));
	// User-defined indexes
	for (map<string, string>::iterator indexIter = m_indexNames.begin(); indexIter != m_indexNames.end(); ++indexIter)
	{
		string indexName = indexIter->first;

		if (isInternalIndex(indexName) == true)
		{
			continue;
		}

		pElem = pRootElem->add_child("extraindex");
		if (pElem == NULL)
		{
			return false;
		}
		addChildElement(pElem, "name", indexIter->first);
		addChildElement(pElem, "location", indexIter->second);
	}
	// Engine channels
	for (map<string, bool>::iterator channelIter = m_engineChannels.begin();
		channelIter != m_engineChannels.end(); ++channelIter)
	{
		// Only save those whose group was collapsed
		if (channelIter->second == false)
		{
			pElem = pRootElem->add_child("channel");
			if (pElem == NULL)
			{
				return false;
			}
			addChildElement(pElem, "name", channelIter->first);
		}
	}
	// User-defined queries
	for (map<string, QueryProperties>::iterator queryIter = m_queries.begin();
		queryIter != m_queries.end(); ++queryIter)
	{
		pElem = pRootElem->add_child("storedquery");
		if (pElem == NULL)
		{
			return false;
		}

		Date aDate;
		unsigned int day, month, year;

		addChildElement(pElem, "name", queryIter->first);
		addChildElement(pElem, "text", queryIter->second.getFreeQuery());
		sprintf(numStr, "%u", queryIter->second.getMaximumResultsCount());
		addChildElement(pElem, "maxresults", numStr);
		bool enable = queryIter->second.getMinimumDate(day, month, year);
		if (year > 0)
		{
			addChildElement(pElem, "enablemindate", (enable ? "YES" : "NO"));
			aDate.set_dmy((Date::Day )day, (Date::Month )month, (Date::Year )year);
			addChildElement(pElem, "mindate", aDate.format_string("%F"));
		}
		enable = queryIter->second.getMaximumDate(day, month, year);
		if (year > 0)
		{
			addChildElement(pElem, "enablemaxdate", (enable ? "YES" : "NO"));
			aDate.set_dmy((Date::Day )day, (Date::Month )month, (Date::Year )year);
			addChildElement(pElem, "maxdate", aDate.format_string("%F"));
		}
		addChildElement(pElem, "index", (queryIter->second.getIndexResults() ? "ALL" : "NONE"));
		addChildElement(pElem, "label", queryIter->second.getLabelName());
	}
	// Labels
	for (set<string>::iterator labelIter = m_labels.begin(); labelIter != m_labels.end(); ++labelIter)
	{
		pElem = pRootElem->add_child("label");
		if (pElem == NULL)
		{
			return false;
		}
		addChildElement(pElem, "name", *labelIter);
	}
	// Ignore robots directives
	addChildElement(pRootElem, "robots", (m_ignoreRobotsDirectives ? "IGNORE" : "OBEY"));
	// New results colour
	pElem = pRootElem->add_child("newresults");
	if (pElem == NULL)
	{
		return false;
	}
	sprintf(numStr, "%u", m_newResultsColourRed);
	addChildElement(pElem, "red", numStr);
	sprintf(numStr, "%u", m_newResultsColourGreen);
	addChildElement(pElem, "green", numStr);
	sprintf(numStr, "%u", m_newResultsColourBlue);
	addChildElement(pElem, "blue", numStr);
	// Enable terms suggestion
	addChildElement(pRootElem, "suggestterms", (m_suggestQueryTerms ? "YES" : "NO"));
	// Mail accounts
	for (set<TimestampedItem>::iterator mailIter = m_mailAccounts.begin(); mailIter != m_mailAccounts.end(); ++mailIter)
	{
		pElem = pRootElem->add_child("mailaccount");
		if (pElem == NULL)
		{
			return false;
		}
		addChildElement(pElem, "name", mailIter->m_name);
		sprintf(numStr, "%ld", mailIter->m_modTime);
		addChildElement(pElem, "mtime", numStr);
	}
	// Locations to index 
	for (set<IndexableLocation>::iterator locationIter = m_indexableLocations.begin(); locationIter != m_indexableLocations.end(); ++locationIter)
	{
		pElem = pRootElem->add_child("indexable");
		if (pElem == NULL)
		{
			return false;
		}
		addChildElement(pElem, "name", locationIter->m_name);
		addChildElement(pElem, "monitor", (locationIter->m_monitor ? "YES" : "NO"));
	}
	// File patterns that are blacklisted
	pElem = pRootElem->add_child("blacklist");
	if (pElem == NULL)
	{
		return false;
	}
	for (set<ustring>::iterator patternIter = m_filePatternsBlackList.begin(); patternIter != m_filePatternsBlackList.end() ; ++patternIter)
	{
		addChildElement(pElem, "pattern", *patternIter);
	}

	// Save to file
	doc.write_to_file_formatted(getConfigurationFileName());

	return true;
}

/// Returns the indexes map, keyed by name.
const map<string, string> &PinotSettings::getIndexes(void) const
{
	return m_indexNames;
}

/// Returns true if the given index is internal.
bool PinotSettings::isInternalIndex(const string &name) const
{
	if ((name == _("My Web Pages")) ||
		(name == _("My Documents")))
	{
		return true;
	}

	return false;		
}

/// Adds a new index.
bool PinotSettings::addIndex(const string &name, const string &location)
{
	map<string, string>::iterator namesMapIter = m_indexNames.find(name);
	if (namesMapIter == m_indexNames.end())
	{
		// Okay, no such index exists
		m_indexIds[1 << m_indexNames.size()] = name;
		m_indexNames[name] = location;

		return true;
	}

	return false;
}

/// Removes an index.
bool PinotSettings::removeIndex(const string &name)
{
	// Remove from the names map
	map<string, string>::iterator namesMapIter = m_indexNames.find(name);
	if (namesMapIter != m_indexNames.end())
	{
		m_indexNames.erase(namesMapIter);

		// Remove from the IDs map
		for (map<unsigned int, string>::iterator idsMapIter = m_indexIds.begin();
			idsMapIter != m_indexIds.end(); ++idsMapIter)
		{
			if (idsMapIter->second == name)
			{
				m_indexIds.erase(idsMapIter);
			}
		}

		return true;
	}

	return false;
}

/// Clears the indexes map.
void PinotSettings::clearIndexes(void)
{
	// Clear both maps, reinsert the internal index
	m_indexNames.clear();
	m_indexIds.clear();
	addIndex(_("My Web Pages"), m_docsIndexLocation);
	addIndex(_("My Documents"), m_daemonIndexLocation);
}

/// Returns an ID that identifies the given index.
unsigned int PinotSettings::getIndexId(const std::string &name)
{
	unsigned int indexId = 0;
	for (map<unsigned int, string>::iterator mapIter = m_indexIds.begin();
		mapIter != m_indexIds.end(); ++mapIter)
	{
		if (mapIter->second == name)
		{
			indexId = mapIter->first;
			break;
		}
	}

	return indexId;
}

/// Returns the name(s) for the given ID.
void PinotSettings::getIndexNames(unsigned int id, std::set<std::string> &names)
{
	names.clear();

	// Make sure there are indexes defined
	if (m_indexNames.empty() == true)
	{
		return;
	}

	unsigned indexId = 1 << (m_indexNames.size() - 1);
	do
	{
		if (id & indexId)
		{
			map<unsigned int, string>::iterator mapIter = m_indexIds.find(indexId);
			if (mapIter != m_indexIds.end())
			{
				// Get the associated name
				names.insert(mapIter->second);
			}
		}
		// Shift to the right
		indexId = indexId >> 1;
	} while (indexId > 0);
}

/// Returns an IndexInterface for the given index location.
IndexInterface *PinotSettings::getIndex(const string &location)
{
	if (location == m_docsIndexLocation)
	{
		return IndexFactory::getIndex("xapian", m_docsIndexLocation);
	}
	else if ((m_enableDBus == true) &&
		(location == m_daemonIndexLocation))
	{
		return IndexFactory::getIndex("dbus", m_daemonIndexLocation);
	}

	return IndexFactory::getIndex("xapian", location);
}

/// Returns the search engines set.
bool PinotSettings::getSearchEngines(set<PinotSettings::Engine> &engines, string channelName) const
{
	if (channelName.empty() == true)
	{
		// Copy the whole list of search engines
		copy(m_engines.begin(), m_engines.end(), inserter(engines, engines.begin()));
	}
	else
	{
		if (m_engineChannels.find(channelName) == m_engineChannels.end())
		{
			// Unknown channel name
			return false;
		}

		// Copy channels that belong to the given channel
		for (set<Engine>::iterator engineIter = m_engines.begin(); engineIter != m_engines.end(); ++engineIter)
		{
			if (engineIter->m_channel == channelName)
			{
				engines.insert(*engineIter);
			}
		}
	}

	return true;
}

/// Returns an ID that identifies the given engine name.
unsigned int PinotSettings::getEngineId(const string &name)
{
	unsigned int engineId = 0;
	for (map<unsigned int, string>::iterator mapIter = m_engineIds.begin();
		mapIter != m_engineIds.end(); ++mapIter)
	{
		if (mapIter->second == name)
		{
			engineId = mapIter->first;
			break;
		}
	}

	return engineId;
}

/// Returns the name for the given ID.
void PinotSettings::getEngineNames(unsigned int id, set<string> &names)
{
	names.clear();

	// Make sure there are search engines defined
	if (m_engines.empty() == true)
	{
		return;
	}

	unsigned engineId = 1 << (m_engines.size() - 1);
	do
	{
		if (id & engineId)
		{
			map<unsigned int, string>::iterator mapIter = m_engineIds.find(engineId);
			if (mapIter != m_engineIds.end())
			{
				// Get the associated name
				names.insert(mapIter->second);
			}
		}
		// Shift to the right
		engineId = engineId >> 1;
	} while (engineId > 0);
}

/// Returns the search engines channels.
map<string, bool> &PinotSettings::getSearchEnginesChannels(void)
{
	return m_engineChannels;
}

/// Returns the queries map, keyed by name.
const map<string, QueryProperties> &PinotSettings::getQueries(void) const
{
	return m_queries;
}

/// Adds a new query.
bool PinotSettings::addQuery(const QueryProperties &properties)
{
	string name = properties.getName();

	map<string, QueryProperties>::iterator queryIter = m_queries.find(name);
	if (queryIter == m_queries.end())
	{
		// Okay, no such query exists
		m_queries[name] = properties;

		return true;
	}

	return false;
}

/// Removes a query.
bool PinotSettings::removeQuery(const string &name)
{
	// Remove from the map
	map<string, QueryProperties>::iterator queryIter = m_queries.find(name);
	if (queryIter != m_queries.end())
	{
		m_queries.erase(queryIter);

		return true;
	}

	return false;
}

/// Clears the queries map.
void PinotSettings::clearQueries(void)
{
	m_queries.clear();
}

/// Returns the labels list.
const set<string> &PinotSettings::getLabels(void) const
{
	return m_labels;
}

/// Adds a new label.
void PinotSettings::addLabel(const string &name)
{
	m_labels.insert(name);
}

/// Removes a label.
void PinotSettings::removeLabel(const string &name)
{
	set<string>::iterator labelIter = m_labels.find(name);
	if (labelIter != m_labels.end())
	{
		m_labels.erase(labelIter);
	}
}

/// Clears the labels list.
void PinotSettings::clearLabels(void)
{
	m_labels.clear();
}

/// Determines if a file matches the blacklist.
bool PinotSettings::isBlackListed(const string &fileName)
{
	if (m_filePatternsBlackList.empty() == true)
	{
		return false;
	}

	// Any pattern matches this file name ?
	for (set<ustring>::iterator patternIter = m_filePatternsBlackList.begin(); patternIter != m_filePatternsBlackList.end() ; ++patternIter)
	{
		if (fnmatch(patternIter->c_str(), fileName.c_str(), FNM_NOESCAPE) == 0)
		{
#ifdef DEBUG
			cout << "PinotSettings::isBlackListed: " << fileName << " matches " << *patternIter << endl;
#endif
			return true;
		}
	}

	return false;
}

PinotSettings::Engine::Engine()
{
}

PinotSettings::Engine::Engine(string name, string type, string option, string channel) :
	m_name(name),
	m_type(type),
	m_option(option),
	m_channel(channel)
{
}

PinotSettings::Engine::Engine(const Engine &other) :
	m_name(other.m_name),
	m_type(other.m_type),
	m_option(other.m_option),
	m_channel(other.m_channel)
{
}

PinotSettings::Engine::~Engine()
{
}

PinotSettings::Engine &PinotSettings::Engine::operator=(const PinotSettings::Engine &other)
{
	if (this != &other)
	{
		m_name = other.m_name;
		m_type = other.m_type;
		m_option = other.m_option;
		m_channel = other.m_channel;
	}

	return *this;
}

bool PinotSettings::Engine::operator<(const PinotSettings::Engine &other) const
{
	if (m_name < other.m_name)
	{
		return true;
	}

	return false;
}

bool PinotSettings::Engine::operator==(const Engine &other) const
{
	if (m_name == other.m_name)
	{
		return true;
	}

	return false;
}

PinotSettings::TimestampedItem::TimestampedItem() :
	m_modTime(0)
{
}

PinotSettings::TimestampedItem::TimestampedItem(const TimestampedItem &other) :
	m_name(other.m_name),
	m_modTime(other.m_modTime)
{
}

PinotSettings::TimestampedItem::~TimestampedItem()
{
}

PinotSettings::TimestampedItem &PinotSettings::TimestampedItem::operator=(const TimestampedItem &other)
{
	if (this != &other)
	{
		m_name = other.m_name;
		m_modTime = other.m_modTime;
	}

	return *this;
}

bool PinotSettings::TimestampedItem::operator<(const TimestampedItem &other) const
{
	if (m_name < other.m_name)
	{
		return true;
	}

	return false;
}

bool PinotSettings::TimestampedItem::operator==(const TimestampedItem &other) const
{
	if (m_name == other.m_name)
	{
		return true;
	}

	return false;
}

PinotSettings::IndexableLocation::IndexableLocation() :
	m_monitor(false)
{
}

PinotSettings::IndexableLocation::IndexableLocation(const IndexableLocation &other) :
	m_monitor(other.m_monitor),
	m_name(other.m_name)
{
}

PinotSettings::IndexableLocation::~IndexableLocation()
{
}

PinotSettings::IndexableLocation &PinotSettings::IndexableLocation::operator=(const IndexableLocation &other)
{
	if (this != &other)
	{
		m_monitor = other.m_monitor;
		m_name = other.m_name;
	}

	return *this;
}

bool PinotSettings::IndexableLocation::operator<(const IndexableLocation &other) const
{
	if (m_name < other.m_name)
	{
		return true;
	}

	return false;
}

bool PinotSettings::IndexableLocation::operator==(const IndexableLocation &other) const
{
	if (m_name == other.m_name)
	{
		return true;
	}

	return false;
}

PinotSettings::CacheProvider::CacheProvider()
{
}

PinotSettings::CacheProvider::CacheProvider(const CacheProvider &other) :
	m_name(other.m_name),
	m_location(other.m_location)
{
	m_protocols.clear();
	copy(other.m_protocols.begin(), other.m_protocols.end(),
		inserter(m_protocols, m_protocols.begin()));
}

PinotSettings::CacheProvider::~CacheProvider()
{
}

PinotSettings::CacheProvider &PinotSettings::CacheProvider::operator=(const CacheProvider &other)
{
	if (this != &other)
	{
		m_name = other.m_name;
		m_location = other.m_location;
		m_protocols.clear();
		copy(other.m_protocols.begin(), other.m_protocols.end(),
			inserter(m_protocols, m_protocols.begin()));
	}

	return *this;
}

bool PinotSettings::CacheProvider::operator<(const CacheProvider &other) const
{
	if (m_name < other.m_name)
	{
		return true;
	}

	return false;
}

bool PinotSettings::CacheProvider::operator==(const CacheProvider &other) const
{
	if (m_name == other.m_name)
	{
		return true;
	}

	return false;
}
