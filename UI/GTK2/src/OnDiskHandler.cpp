/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <fstream>

#include "config.h"
#include "NLS.h"
#include "MIMEScanner.h"
#include "StringManip.h"
#include "Url.h"
#include "XapianDatabase.h"
#include "TokenizerFactory.h"
#include "CrawlHistory.h"
#include "FileCollector.h"
#include "PinotUtils.h"
#include "OnDiskHandler.h"

using namespace std;
using namespace SigC;

OnDiskHandler::OnDiskHandler() :
	MonitorHandler(),
	m_history(PinotSettings::getInstance().m_historyDatabase),
	m_index(PinotSettings::getInstance().m_daemonIndexLocation)
{
	m_index.setStemmingMode(WritableIndexInterface::STORE_BOTH);
}

OnDiskHandler::~OnDiskHandler()
{
}

bool OnDiskHandler::indexFile(const string &fileName, bool alwaysUpdate)
{
	string url(string("file://") + fileName);
	Url urlObj(url);
	set<string> labels;
	bool indexedFile = false;

	if ((m_index.isGood() == false) || 
		(fileName.empty() == true))
	{
		return false;
	}

	// Has this file been indexed already ?
	unsigned int docId = m_index.hasDocument(url);
	if ((docId > 0) &&
		(alwaysUpdate == false))
	{
		// No need to update
		return true;
	}

	DocumentInfo docInfo(url, url, MIMEScanner::scanUrl(urlObj), "");

	FileCollector fileCollector;
	Document *pDoc = fileCollector.retrieveUrl(docInfo);
	if (pDoc == NULL)
	{
#ifdef DEBUG
		cout << "OnDiskHandler::indexFile: couldn't download " << url << endl;
#endif
		return false;
	}

	// Get an ad hoc tokenizer for the message
	Tokenizer *pTokenizer = TokenizerFactory::getTokenizerByType(docInfo.getType(), pDoc);
	if (pTokenizer == NULL)
	{
#ifdef DEBUG
		cout << "OnDiskHandler::indexFile: no tokenizer for type " << docInfo.getType() << endl;
#endif
		delete pDoc;
		return false;
	}

	// Index or update ?
	if (docId == 0)
	{
		indexedFile = m_index.indexDocument(*pTokenizer, labels, docId);
	}
	else
	{
		indexedFile = m_index.updateDocument(docId, *pTokenizer);
	}

	if (indexedFile == true)
	{
		IndexedDocument indexedDocInfo(docInfo.getTitle(),
			XapianDatabase::buildUrl(PinotSettings::getInstance().m_daemonIndexLocation, docId),
			docInfo.getLocation(), docInfo.getType(), docInfo.getLanguage());

		// Signal
		m_signalUpdate(indexedDocInfo, docId, _("My Computer"));
	}
#ifdef DEBUG
	else cout << "OnDiskHandler::indexFile: couldn't index " << url << endl;
#endif

	delete pTokenizer;
	delete pDoc;

	return indexedFile;
}

void OnDiskHandler::initialize(void)
{
	map<unsigned int, string> sources;

	// Get the map of indexable locations
	set<PinotSettings::TimestampedItem> &indexableLocations = PinotSettings::getInstance().m_indexableLocations;
	for (set<PinotSettings::TimestampedItem>::iterator dirIter = indexableLocations.begin();
		dirIter != indexableLocations.end(); ++dirIter)
	{
		m_locations.insert(dirIter->m_name);
	}

	// Unindex documents that belong to sources that no longer exist
	if (m_history.getSources(sources) > 0)
	{
		for(map<unsigned int, string>::const_iterator sourceIter = sources.begin();
			sourceIter != sources.end(); ++sourceIter)
		{
			unsigned int sourceId = sourceIter->first;

			// Is this a file and does it still exist ?
			if (sourceIter->second.substr(0, 7) != "file://")
			{
				continue;
			}

			if (m_locations.find(sourceIter->second.substr(7)) == m_locations.end())
			{
				char sourceStr[64];

				snprintf(sourceStr, 64, "SOURCE%u", sourceId);

#ifdef DEBUG
				cout << "OnDiskHandler::initialize: " << sourceIter->second
					<< ", source " << sourceId << " was removed" << endl;
#endif
				// All documents with this label will be unindexed
				m_index.unindexDocuments(sourceStr);

				// Delete the source itself and all its items
				m_history.deleteSource(sourceId);
				m_history.deleteItems(sourceId);
			}
#ifdef DEBUG
			else cout << "OnDiskHandler::initialize: " << sourceIter->second
				<< " still configured for monitoring" << endl;
#endif
		}
	}
}

void OnDiskHandler::flushIndex(void)
{
	m_index.flush();
}

bool OnDiskHandler::fileExists(const string &fileName)
{
#ifdef DEBUG
	cout << "OnDiskHandler::fileExists: " << fileName << endl;
#endif
}

bool OnDiskHandler::fileCreated(const string &fileName)
{
#ifdef DEBUG
	cout << "OnDiskHandler::fileCreated: " << fileName << endl;
#endif
	// The file shouldn't exist in the index, but if it does for some reason, don't update it
	return indexFile(fileName, false);
}

bool OnDiskHandler::fileModified(const string &fileName)
{
#ifdef DEBUG
	cout << "OnDiskHandler::fileModified: " << fileName << endl;
#endif
	// Update the file, or index if necessary
	return indexFile(fileName, true);
}

bool OnDiskHandler::fileMoved(const string &fileName, const string &previousFileName)
{
#ifdef DEBUG
	cout << "OnDiskHandler::fileMoved: " << fileName << endl;
#endif
	unsigned int oldDocId = m_index.hasDocument(string("file://") + previousFileName);
	if (oldDocId > 0)
	{
		DocumentInfo docInfo;

		m_index.getDocumentInfo(oldDocId, docInfo);

		// Has the destination file been indexed too ?
		unsigned int docId = m_index.hasDocument(string("file://") + fileName);
		if (docId > 0)
		{
			// Unindex it
			m_index.unindexDocument(docId);
		}

		// Change the location
		docInfo.setLocation(string("file://") + fileName);
		return m_index.updateDocumentInfo(oldDocId, docInfo);
	}

	return false; 
}

bool OnDiskHandler::fileDeleted(const string &fileName)
{
#ifdef DEBUG
	cout << "OnDiskHandler::fileDeleted: " << fileName << endl;
#endif
	unsigned int docId = m_index.hasDocument(string("file://") + fileName);
	if (docId > 0)
	{
		// Unindex the file
		return m_index.unindexDocument(docId);
	}

	return false;
}
