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

#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <set>
#include <iostream>
#include <fstream>

#include "config.h"
#include "NLS.h"
#include "MIMEScanner.h"
#include "StringManip.h"
#include "Url.h"
#include "XapianDatabase.h"
#include "TokenizerFactory.h"
#include "FileCollector.h"
#include "OnDiskHandler.h"

using namespace std;
using namespace SigC;

OnDiskHandler::OnDiskHandler() :
	MonitorHandler(),
	m_history(PinotSettings::getInstance().m_historyDatabase),
	m_index(PinotSettings::getInstance().m_daemonIndexLocation)
{
	pthread_mutex_init(&m_mutex, NULL);
	m_index.setStemmingMode(IndexInterface::STORE_BOTH);
}

OnDiskHandler::~OnDiskHandler()
{
	pthread_mutex_destroy(&m_mutex);
}

bool OnDiskHandler::indexFile(const string &fileName, bool alwaysUpdate, unsigned int &sourceId)
{
	string location(string("file://") + fileName);
	Url urlObj(location);
	set<string> labels;
	bool indexedFile = false;

	if ((m_index.isGood() == false) || 
		(fileName.empty() == true))
	{
		return false;
	}

	// Has this file been indexed already ?
	unsigned int docId = m_index.hasDocument(location);
	if ((docId > 0) &&
		(alwaysUpdate == false))
	{
		// No need to update
		return true;
	}

	// What source does it belong to ?
	for(map<unsigned int, string>::const_iterator sourceIter = m_fileSources.begin();
		sourceIter != m_fileSources.end(); ++sourceIter)
	{
		sourceId = sourceIter->first;

		if (sourceIter->second.length() < location.length())
		{
			// Skip
			continue;
		}

		if (sourceIter->second.substr(0, location.length()) == location)
		{
			char sourceStr[64];

			// That's the one
			snprintf(sourceStr, 64, "SOURCE%u", sourceIter->first);
			labels.insert(sourceStr);
			break;
		}
	}

	DocumentInfo docInfo(urlObj.getFile(), location, MIMEScanner::scanUrl(urlObj), "");

	FileCollector fileCollector;
	Document *pDoc = fileCollector.retrieveUrl(docInfo);
	if (pDoc == NULL)
	{
#ifdef DEBUG
		cout << "OnDiskHandler::indexFile: couldn't download " << location << endl;
#endif

		// The file  couldn't be downloaded but exists nonetheless !
		pDoc = new Document(docInfo);
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

#ifdef DEBUG
	if (indexedFile == false)
	{
		cout << "OnDiskHandler::indexFile: couldn't index " << location << endl;
	}
#endif

	delete pTokenizer;
	delete pDoc;

	return indexedFile;
}

bool OnDiskHandler::replaceFile(unsigned int docId, DocumentInfo &docInfo)
{
	// Has the destination file been indexed too ?
	unsigned int destDocId = m_index.hasDocument(docInfo.getLocation());
	if (destDocId > 0)
	{
		// Unindex it
		m_index.unindexDocument(destDocId);
	}

	// Update the document info
	return m_index.updateDocumentInfo(docId, docInfo);
}

void OnDiskHandler::initialize(void)
{
	set<string> directories;

	// Get the map of indexable locations
	set<PinotSettings::IndexableLocation> &indexableLocations = PinotSettings::getInstance().m_indexableLocations;
	for (set<PinotSettings::IndexableLocation>::iterator dirIter = indexableLocations.begin();
		dirIter != indexableLocations.end(); ++dirIter)
	{
		directories.insert(dirIter->m_name);
	}

	// Unindex documents that belong to sources that no longer exist
	if (m_history.getSources(m_fileSources) > 0)
	{
		for(map<unsigned int, string>::const_iterator sourceIter = m_fileSources.begin();
			sourceIter != m_fileSources.end(); ++sourceIter)
		{
			unsigned int sourceId = sourceIter->first;

			if (sourceIter->second.substr(0, 7) != "file://")
			{
				// Skip
				continue;
			}

			// Is this an indexable location ?
			if (directories.find(sourceIter->second.substr(7)) == directories.end())
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
				<< " is still configured" << endl;
#endif
		}
	}
}

void OnDiskHandler::flushIndex(void)
{
	pthread_mutex_lock(&m_mutex);
	m_index.flush();
	pthread_mutex_unlock(&m_mutex);
}

bool OnDiskHandler::fileExists(const string &fileName)
{
	// Nothing to do here
	return true;
}

bool OnDiskHandler::fileCreated(const string &fileName)
{
	unsigned int sourceId;
	bool handledEvent = false;

#ifdef DEBUG
	cout << "OnDiskHandler::fileCreated: " << fileName << endl;
#endif
	pthread_mutex_lock(&m_mutex);
	// The file shouldn't exist in the index, but if it does for some reason, don't update it
	handledEvent = indexFile(fileName, false, sourceId);
	if (handledEvent == true)
	{
		m_history.insertItem("file://" + fileName, CrawlHistory::CRAWLED, sourceId, time(NULL));
	}
	pthread_mutex_unlock(&m_mutex);

	return handledEvent;
}

bool OnDiskHandler::fileModified(const string &fileName)
{
	unsigned int sourceId;
	bool handledEvent = false;

#ifdef DEBUG
	cout << "OnDiskHandler::fileModified: " << fileName << endl;
#endif
	pthread_mutex_lock(&m_mutex);
	// Update the file, or index if necessary
	handledEvent = indexFile(fileName, true, sourceId);
	if (handledEvent == true)
	{
		m_history.updateItem("file://" + fileName, CrawlHistory::CRAWLED, time(NULL));
	}
	pthread_mutex_unlock(&m_mutex);

	return handledEvent;
}

bool OnDiskHandler::fileMoved(const string &fileName, const string &previousFileName)
{
	bool handledEvent = false;

#ifdef DEBUG
	cout << "OnDiskHandler::fileMoved: " << fileName << endl;
#endif
	pthread_mutex_lock(&m_mutex);
	unsigned int oldDocId = m_index.hasDocument(string("file://") + previousFileName);
	if (oldDocId > 0)
	{
		DocumentInfo docInfo;

		if (m_index.getDocumentInfo(oldDocId, docInfo) == true)
		{
			// Change the location
			docInfo.setLocation(string("file://") + fileName);

			handledEvent = replaceFile(oldDocId, docInfo);
		}
	}
	pthread_mutex_unlock(&m_mutex);

	return handledEvent;
}

bool OnDiskHandler::directoryMoved(const string &dirName,
	const string &previousDirName)
{
	set<unsigned int> docIdList;
	bool handledEvent = false;

#ifdef DEBUG
	cout << "OnDiskHandler::directoryMoved: " << dirName << endl;
#endif
	pthread_mutex_lock(&m_mutex);
	if (m_index.listDocumentsInDirectory(previousDirName, docIdList) == true)
	{
		for (set<unsigned int>::const_iterator iter = docIdList.begin();
			iter != docIdList.end(); ++iter)
		{
			DocumentInfo docInfo;

			if (m_index.getDocumentInfo(*iter, docInfo) == true)
			{
				string newLocation(docInfo.getLocation());

				string::size_type pos = newLocation.find(previousDirName);
				if (pos != string::npos)
				{
					newLocation.replace(pos, previousDirName.length(), dirName);

					// Change the location
					docInfo.setLocation(newLocation);

					replaceFile(*iter, docInfo);
				}
			}
		}

		handledEvent = true;
	}
#ifdef DEBUG
	else cout << "OnDiskHandler::directoryMoved: no documents in " << previousDirName << endl;
#endif
	pthread_mutex_unlock(&m_mutex);

	return handledEvent;
}

bool OnDiskHandler::fileDeleted(const string &fileName)
{
	bool handledEvent = false;

#ifdef DEBUG
	cout << "OnDiskHandler::fileDeleted: " << fileName << endl;
#endif
	pthread_mutex_lock(&m_mutex);
	unsigned int docId = m_index.hasDocument(string("file://") + fileName);
	if (docId > 0)
	{
		// Unindex the file
		handledEvent = m_index.unindexDocument(docId);
		if (handledEvent == true)
		{
			m_history.deleteItem(string("file://") + fileName);
		}
	}
	pthread_mutex_unlock(&m_mutex);

	return handledEvent;
}

bool OnDiskHandler::directoryDeleted(const string &dirName)
{
	set<unsigned int> docIdList;
	bool handledEvent = false;

#ifdef DEBUG
	cout << "OnDiskHandler::directoryDeleted: " << dirName << endl;
#endif
	pthread_mutex_lock(&m_mutex);
	if (m_index.listDocumentsInDirectory(dirName, docIdList) == true)
	{
		for (set<unsigned int>::const_iterator iter = docIdList.begin();
			iter != docIdList.end(); ++iter)
		{
			DocumentInfo docInfo;

			if ((m_index.getDocumentInfo(*iter, docInfo) == true) &&
				(m_index.unindexDocument(*iter) == true))
			{
				m_history.deleteItem(docInfo.getLocation());
			}
		}

		handledEvent = true;
	}
#ifdef DEBUG
	else cout << "OnDiskHandler::directoryDeleted: no documents in " << dirName << endl;
#endif
	pthread_mutex_unlock(&m_mutex);

	return handledEvent;
}

