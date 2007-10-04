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
#include "FilterWrapper.h"
#include "XapianDatabase.h"
#include "FileCollector.h"
#include "PinotSettings.h"
#include "OnDiskHandler.h"

using namespace std;

OnDiskHandler::OnDiskHandler() :
	MonitorHandler(),
	m_history(PinotSettings::getInstance().getHistoryDatabaseName()),
	m_index(PinotSettings::getInstance().m_daemonIndexLocation)
{
	pthread_mutex_init(&m_mutex, NULL);
}

OnDiskHandler::~OnDiskHandler()
{
	pthread_mutex_destroy(&m_mutex);
}

bool OnDiskHandler::fileMoved(const string &fileName, const string &previousFileName,
	IndexInterface::NameType type)
{
	set<unsigned int> docIdList;
	bool handledEvent = false;

#ifdef DEBUG
	cout << "OnDiskHandler::fileMoved: " << fileName << endl;
#endif
	pthread_mutex_lock(&m_mutex);
	// Get a list of documents in that directory/file
	if (type == IndexInterface::BY_FILE)
	{
		m_index.listDocuments(string("file://") + previousFileName, docIdList, type);
	}
	else
	{
		m_index.listDocuments(previousFileName, docIdList, type);
	}
	// ...and the directory/file itself
	unsigned int baseDocId = m_index.hasDocument(string("file://") + previousFileName);
	if (baseDocId > 0)
	{
		docIdList.insert(baseDocId);
	}
	if (docIdList.empty() == false)
	{
		for (set<unsigned int>::const_iterator iter = docIdList.begin();
			iter != docIdList.end(); ++iter)
		{
			DocumentInfo docInfo;

#ifdef DEBUG
			cout << "OnDiskHandler::fileMoved: moving " << *iter << endl;
#endif
			if (m_index.getDocumentInfo(*iter, docInfo) == true)
			{
				string newLocation(docInfo.getLocation());

				if (baseDocId == *iter)
				{
					Url previousUrlObj(previousFileName), urlObj(fileName);

					// Update the title if it was the directory/file name
					if (docInfo.getTitle() == previousUrlObj.getFile())
					{
						docInfo.setTitle(urlObj.getFile());
					}
				}

				string::size_type pos = newLocation.find(previousFileName);
				if (pos != string::npos)
				{
					newLocation.replace(pos, previousFileName.length(), fileName);

					// Change the location
					docInfo.setLocation(newLocation);

					handledEvent = replaceFile(*iter, docInfo);
#ifdef DEBUG
					cout << "OnDiskHandler::fileMoved: moved " << *iter << ", " << docInfo.getLocation() << endl;
#endif
				}
#ifdef DEBUG
				else cout << "OnDiskHandler::fileMoved: skipping " << newLocation << endl;
#endif
			}
		}
	}
#ifdef DEBUG
	else cout << "OnDiskHandler::fileMoved: no documents in " << previousFileName << endl;
#endif
	pthread_mutex_unlock(&m_mutex);

	return handledEvent;
}

bool OnDiskHandler::fileDeleted(const string &fileName, IndexInterface::NameType type)
{
	set<unsigned int> docIdList;
	string location(string("file://") + fileName);
	bool unindexedDocs = false, handledEvent = false;

#ifdef DEBUG
	cout << "OnDiskHandler::fileDeleted: " << fileName << endl;
#endif
	pthread_mutex_lock(&m_mutex);
	// Unindex all of the directory/file's documents
	if (type == IndexInterface::BY_FILE)
	{
		unindexedDocs = m_index.unindexDocuments(location, type);
	}
	else
	{
		unindexedDocs = m_index.unindexDocuments(fileName, type);
	}
	if (unindexedDocs == true)
	{
		// ...as well as the actual directory/file
		m_index.unindexDocument(location);

		m_history.deleteItems(location);
		handledEvent = true;
	}
	pthread_mutex_unlock(&m_mutex);

	return handledEvent;
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

	// Is it black-listed ?
	if (PinotSettings::getInstance().isBlackListed(fileName) == true)
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
			snprintf(sourceStr, 64, "X-SOURCE%u", sourceIter->first);
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

	FilterWrapper wrapFilter(&m_index);

	// Get an ad hoc tokenizer for the message
	// Index or update ?
	if (docId == 0)
	{
		indexedFile = wrapFilter.indexDocument(*pDoc, labels, docId);
	}
	else
	{
		indexedFile = wrapFilter.updateDocument(*pDoc, docId);
	}

#ifdef DEBUG
	if (indexedFile == false)
	{
		cout << "OnDiskHandler::indexFile: couldn't index " << location << endl;
	}
#endif

	delete pDoc;

	return indexedFile;
}

bool OnDiskHandler::replaceFile(unsigned int docId, DocumentInfo &docInfo)
{
	FilterWrapper wrapFilter(&m_index);

	// Unindex the destination file
	wrapFilter.unindexDocument(docInfo.getLocation());

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

				snprintf(sourceStr, 64, "X-SOURCE%u", sourceId);

#ifdef DEBUG
				cout << "OnDiskHandler::initialize: " << sourceIter->second
					<< ", source " << sourceId << " was removed" << endl;
#endif
				// All documents with this label will be unindexed
				if (m_index.unindexDocuments(sourceStr, IndexInterface::BY_LABEL) == true)
				{
					// Delete the source itself and all its items
					m_history.deleteSource(sourceId);
					m_history.deleteItems(sourceId);
				}
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
	return fileMoved(fileName, previousFileName, IndexInterface::BY_FILE);
}

bool OnDiskHandler::directoryMoved(const string &dirName,
	const string &previousDirName)
{
	return fileMoved(dirName, previousDirName, IndexInterface::BY_DIRECTORY);
}

bool OnDiskHandler::fileDeleted(const string &fileName)
{
	return fileDeleted(fileName, IndexInterface::BY_FILE);
}

bool OnDiskHandler::directoryDeleted(const string &dirName)
{
	return fileDeleted(dirName, IndexInterface::BY_DIRECTORY);
}

