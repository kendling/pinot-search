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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <algorithm>
#include <iostream>

#include <glibmm/convert.h>
#include <libxml++/parsers/domparser.h>
#include <libxml++/nodes/node.h>
#include <libxml++/nodes/textnode.h>

#include "config.h"
#include "Languages.h"
#include "NLS.h"
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

PinotSettings::PinotSettings() :
	m_browseResults(false),
	m_xPos(0),
	m_yPos(0),
	m_width(0),
	m_height(0),
	m_panePos(-1),
	m_ignoreRobotsDirectives(false),
	m_suggestQueryTerms(true),
	m_newResultsColour("red"),
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
			cerr << _("Couldn't create pinot directory at") << " "
				<< directoryName << endl;
		}
	}

	// This is where the internal indices live
	m_indexLocation = directoryName;
	m_indexLocation += "/index";
	m_mailIndexLocation = directoryName;
	m_mailIndexLocation += "/mail";

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

bool PinotSettings::load(void)
{
	// Clear lists
	m_indexNames.clear();
	m_indexIds.clear();
	m_engines.clear();
	m_engineIds.clear();
	m_queries.clear();
	m_labels.clear();
	m_mailAccounts.clear();

	// Load the configuration file
	string fileName = getConfigurationFileName();
	if ((m_firstRun == false) &&
		(loadConfiguration(fileName) == false))
	{
		cerr << _("Failed to parse configuration file") << " "
			<< fileName << endl;
	}
	// Internal indices
	addIndex(_("My Documents"), m_indexLocation);
	addIndex(_("My Email"), m_mailIndexLocation);
	// Add default labels on the first run
	if (m_firstRun == true)
	{
		m_labels.insert(_("Important"));
		m_labels.insert(_("New"));
		m_labels.insert(_("Personal"));
	}

	// Some search engines are hardcoded
#ifdef HAS_GOOGLEAPI
	m_engineIds[1 << m_engines.size()] = "Google API";
	m_engines.insert(Engine("Google API", "googleapi", "", "Web Services"));
	m_engineChannels.insert("Web Services");
#endif
#ifdef HAS_OSAPI
	m_engineIds[1 << m_engines.size()] = "ObjectsSearch API";
	m_engines.insert(Engine("ObjectsSearch API", "objectssearchapi", "", "Web Services"));
	m_engineChannels.insert("Web Services");
#endif
	m_engineIds[1 << m_engines.size()] = "Xapian";
	m_engines.insert(Engine("Xapian", "xapian", "", ""));

	return true;
}

bool PinotSettings::loadConfiguration(const std::string &fileName)
{
	struct stat fileStat;
	bool success = true;

	if ((stat(fileName.c_str(), &fileStat) != 0) ||
		(!S_ISREG(fileStat.st_mode)))
	{
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

				string nodeName = pElem->get_name();
				string nodeContent = getElementContent(pElem);
				if (nodeName == "googleapikey")
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
				else if (nodeName == "storedquery")
				{
					loadQueries(pElem);
				}
				else if (nodeName == "results")
				{
					loadResults(pElem);
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
			}
		}
	}
	catch (const std::exception& ex)
	{
		cerr << _("Couldn't parse configuration file") << ": "
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
		else if (nodeName == "and")
		{
			queryProps.setAndWords(nodeContent);
		}
		else if (nodeName == "phrase")
		{
			queryProps.setPhrase(nodeContent);
		}
		else if (nodeName == "any")
		{
			queryProps.setAnyWords(nodeContent);
		}
		else if (nodeName == "not")
		{
			queryProps.setNotWords(nodeContent);
		}
		else if (nodeName == "language")
		{
			queryProps.setLanguage(Languages::toLocale(nodeContent));
		}
		else if (nodeName == "hostfilter")
		{
			queryProps.setHostFilter(nodeContent);
		}
		else if (nodeName == "filefilter")
		{
			queryProps.setFileFilter(nodeContent);
		}
		else if (nodeName == "labelfilter")
		{
			queryProps.setLabelFilter(nodeContent);
		}
		else if (nodeName == "maxresults")
		{
			int count = atoi(nodeContent.c_str());
			queryProps.setMaximumResultsCount((unsigned int)max(count, 10));
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
		m_queries[queryProps.getName()] = queryProps;
	}

	return true;
}

bool PinotSettings::loadResults(const Element *pElem)
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
		if (nodeName == "viewmode")
		{
			if (nodeContent == "SOURCE")
			{
				m_browseResults = false;
			}
			else
			{
				m_browseResults = true;
			}
		}
		else if (nodeName == "browser")
		{
			m_browserCommand = nodeContent;
		}
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
			m_newResultsColour.set_red(value);
		}
		else if (nodeName == "green")
		{
			m_newResultsColour.set_green(value);
		}
		else if (nodeName == "blue")
		{
			m_newResultsColour.set_blue(value);
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

	MailAccount mailAccount;

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
		else if (nodeName == "type")
		{
			mailAccount.m_type = nodeContent;
		}
		else if (nodeName == "mtime")
		{
			mailAccount.m_modTime = (time_t)atoi(nodeContent.c_str());
		}
		else if (nodeName == "mindate")
		{
			mailAccount.m_lastMessageTime = (time_t)atoi(nodeContent.c_str());
		}
		else if (nodeName == "size")
		{
			mailAccount.m_size = (off_t)atoi(nodeContent.c_str());
		}
	}

	if (mailAccount.m_name.empty() == false)
	{
		m_mailAccounts.insert(mailAccount);
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
					m_engineChannels.insert(engineChannel);
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
	// User-defined queries
	for (map<string, QueryProperties>::iterator queryIter = m_queries.begin();
		queryIter != m_queries.end(); ++queryIter)
	{
		pElem = pRootElem->add_child("storedquery");
		if (pElem == NULL)
		{
			return false;
		}
		addChildElement(pElem, "name", queryIter->first);
		addChildElement(pElem, "type", "FIXED");
		addChildElement(pElem, "and", queryIter->second.getAndWords());
		addChildElement(pElem, "phrase", queryIter->second.getPhrase());
		addChildElement(pElem, "any", queryIter->second.getAnyWords());
		addChildElement(pElem, "not", queryIter->second.getNotWords());
		addChildElement(pElem, "hostfilter", queryIter->second.getHostFilter());
		addChildElement(pElem, "filefilter", queryIter->second.getFileFilter());
		addChildElement(pElem, "labelfilter", queryIter->second.getLabelFilter());
		addChildElement(pElem, "language", Languages::toEnglish(queryIter->second.getLanguage()));
		sprintf(numStr, "%u", queryIter->second.getMaximumResultsCount());
		addChildElement(pElem, "maxresults", numStr);
		addChildElement(pElem, "index", (queryIter->second.getIndexResults() ? "ALL" : "NONE"));
		addChildElement(pElem, "label", queryIter->second.getLabelName());
	}
	pElem = pRootElem->add_child("results");
	if (pElem == NULL)
	{
		return false;
	}
	// Results view options
	addChildElement(pElem, "viewmode", (m_browseResults ? "BROWSE" : "SOURCE"));
	addChildElement(pElem, "browser", m_browserCommand);
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
	sprintf(numStr, "%u", m_newResultsColour.get_red());
	addChildElement(pElem, "red", numStr);
	sprintf(numStr, "%u", m_newResultsColour.get_green());
	addChildElement(pElem, "green", numStr);
	sprintf(numStr, "%u", m_newResultsColour.get_blue());
	addChildElement(pElem, "blue", numStr);
	// Enable terms suggestion
	addChildElement(pRootElem, "suggestterms", (m_suggestQueryTerms ? "YES" : "NO"));
	// Mail accounts
	for (set<MailAccount>::iterator mailIter = m_mailAccounts.begin(); mailIter != m_mailAccounts.end(); ++mailIter)
	{
		pElem = pRootElem->add_child("mailaccount");
		if (pElem == NULL)
		{
			return false;
		}
		addChildElement(pElem, "name", mailIter->m_name);
		addChildElement(pElem, "type", mailIter->m_type);
		sprintf(numStr, "%d", mailIter->m_modTime);
		addChildElement(pElem, "mtime", numStr);
		sprintf(numStr, "%d", mailIter->m_lastMessageTime);
		addChildElement(pElem, "mindate", numStr);
		sprintf(numStr, "%d", mailIter->m_size);
		addChildElement(pElem, "size", numStr);
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
bool PinotSettings::isInternalIndex(const string &indexName) const
{
	if ((indexName == _("My Documents")) ||
		(indexName == _("My Email")))
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
	addIndex(_("My Documents"), m_indexLocation);
	addIndex(_("My Email"), m_mailIndexLocation);
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
const set<string> &PinotSettings::getSearchEnginesChannels(void) const
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

PinotSettings::Engine::Engine()
{
}

PinotSettings::Engine::Engine(string name, string type, string option, string channel)
{
	m_name = name;
	m_type = type;
	m_option = option;
	m_channel = channel;
}

PinotSettings::Engine::~Engine()
{
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

PinotSettings::MailAccount::MailAccount()
{
	m_modTime = m_lastMessageTime = (time_t)0;
	m_size = 0;
}

PinotSettings::MailAccount::MailAccount(const MailAccount &other)
{
	m_name = other.m_name;
	m_type = other.m_type;
	m_modTime = other.m_modTime;
	m_lastMessageTime = other.m_lastMessageTime;
	m_size = other.m_size;
}

PinotSettings::MailAccount::~MailAccount()
{
}

bool PinotSettings::MailAccount::operator<(const MailAccount &other) const
{
	if (m_name < other.m_name)
	{
		return true;
	}

	return false;
}

bool PinotSettings::MailAccount::operator==(const MailAccount &other) const
{
	if (m_name == other.m_name)
	{
		return true;
	}

	return false;
}
