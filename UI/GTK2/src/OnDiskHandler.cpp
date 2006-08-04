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
#include "FileCollector.h"
#include "PinotUtils.h"
#include "OnDiskHandler.h"

using namespace std;
using namespace SigC;

OnDiskHandler::OnDiskHandler() :
	MonitorHandler(),
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

bool OnDiskHandler::getLocations(set<string> &newLocations,
	set<string> &locationsToRemove)
{
	newLocations.clear();
	locationsToRemove.clear();

	// Take advantage of this call to flush the index
	m_index.flush();

	copy(m_locations.begin(), m_locations.end(),
		inserter(locationsToRemove, locationsToRemove.begin()));

	// Get the indexable locations map
	set<PinotSettings::TimestampedItem> &indexableLocations = PinotSettings::getInstance().m_indexableLocations;
	for (set<PinotSettings::TimestampedItem>::iterator dirIter = indexableLocations.begin();
		dirIter != indexableLocations.end(); ++dirIter)
	{
		// Is this a known location ?
		set<string>::iterator locationIter = m_locations.find(dirIter->m_name);
		if (locationIter == m_locations.end())
		{
			// No, it is new
			m_locations.insert(dirIter->m_name);
			newLocations.insert(dirIter->m_name);
		}
		else
		{
			// Since it's a known location, we'd better not remove it
			set<string>::iterator removeIter = locationsToRemove.find(*locationIter);
			if (removeIter != locationsToRemove.end())
			{
				locationsToRemove.erase(removeIter);
			}
		}
	}

	// Locations in locationsToRemove have to be removed
	for (set<string>::iterator removeIter = locationsToRemove.begin();
		removeIter != locationsToRemove.end(); ++removeIter)
	{
		set<string>::iterator locationIter = m_locations.find(*removeIter);
		if (locationIter != m_locations.end())
		{
			m_locations.erase(locationIter);
		}
	}

#ifdef DEBUG
	cout << "OnDiskHandler::getLocations: " << m_locations.size() << " locations, "
		<< newLocations.size() << " new, " << locationsToRemove.size() << " to be removed" << endl;
#endif

	if ((newLocations.empty() == false) ||
		(locationsToRemove.empty() == false))
	{
		return true;
	}

	return false;
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

