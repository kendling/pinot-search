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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <fstream>

#include "config.h"
#include "NLS.h"
#include "StringManip.h"
#include "Timer.h"
#include "TimeConverter.h"
#include "Url.h"
#include "FilterFactory.h"
#include "FilterUtils.h"
#include "FilterWrapper.h"
#include "XapianDatabase.h"
#include "MboxHandler.h"

using namespace std;
using namespace SigC;

MboxHandler::MboxHandler() :
	MonitorHandler(),
	m_history(PinotSettings::getInstance().m_historyDatabase),
	m_index(PinotSettings::getInstance().m_daemonIndexLocation),
	m_sourceId(0)
{
	// Does the email source exist ?
	if (m_history.hasSource("My Email", m_sourceId) == false)
	{
		// Create it
		m_sourceId = m_history.insertSource("My Email");
	}
#ifdef DEBUG
	cout << "MboxHandler: My Email source ID is " << m_sourceId << endl;
#endif
}

MboxHandler::~MboxHandler()
{
}

bool MboxHandler::checkMailAccount(const string &fileName, PinotSettings::TimestampedItem &mailAccount)
{
	struct stat fileStat;
	CrawlHistory::CrawlStatus status;
	time_t lastModDate = 0;
	mailAccount.m_name = fileName;

	// Ensure it's one of our mail accounts
	set<PinotSettings::TimestampedItem> &mailAccounts = PinotSettings::getInstance().m_mailAccounts;
	set<PinotSettings::TimestampedItem>::iterator mailIter = mailAccounts.find(mailAccount);
	if (mailIter == mailAccounts.end())
	{
		// It doesn't seem to be
#ifdef DEBUG
		cout << "MboxHandler::checkMailAccount: not one of " << mailAccounts.size() << " accounts" << endl;
#endif
		return false;
	}

	// Find out when it was last modified
	if ((stat(fileName.c_str(), &fileStat) == 0) &&
		(!S_ISREG(fileStat.st_mode)))
	{
		// This is not a file !
		return false;
	}

	// Is there a record for this mail account ?
	if (m_history.hasItem("file://" + fileName, status, lastModDate) == true)
	{
		if (fileStat.st_mtime <= lastModDate)
		{
			// No change since last time...
#ifdef DEBUG
			cout << "MboxHandler::checkMailAccount: not modified since last time ("
				<< lastModDate << ">" << fileStat.st_mtime << ")" << endl;
#endif
			return false;
		}
	}
	else
	{
		m_history.insertItem("file://" + fileName, CrawlHistory::CRAWLING, m_sourceId, time(NULL));
	}

	// Update this mail account's properties
	mailAccount = (*mailIter);
	mailAccount.m_modTime = fileStat.st_mtime;

	return true;
}

bool MboxHandler::indexMessages(const string &fileName, PinotSettings::TimestampedItem &mailAccount,
	off_t mboxOffset)
{
	string sourceLabel("mailbox://");

	// Come up with a label for this mbox file's messages
	sourceLabel += fileName;

	if (m_index.isGood() == false)
	{
		cerr << "MboxHandler::indexMessages: couldn't get mail index" << endl;
		return false;
	}

	// Get the mbox filter
	Dijon::Filter *pFilter = Dijon::FilterFactory::getFilter("application/mbox");
	if (pFilter == NULL)
	{
		return false;
	}

	Document emptyDoc;
	emptyDoc.setLocation(string("file://") + fileName);
	if (FilterUtils::feedFilter(emptyDoc, pFilter) == false)
	{
		delete pFilter;
		return false;
	}

	bool indexedFile = parseMailAccount(pFilter, sourceLabel);

	delete pFilter;

	// Flush the index
	m_index.flush();

	// Update this mail account's record
	m_history.updateItem("file://" + fileName, CrawlHistory::CRAWLED, mailAccount.m_modTime);

	return indexedFile;
}

bool MboxHandler::parseMailAccount(Dijon::Filter *pFilter, const string &sourceLabel)
{
	set<unsigned int> docIdList;
	set<string> labels;
	char sourceStr[64];
	unsigned int docNum = 0;
	bool indexedFile = false;
#ifdef DEBUG
	Timer timer;
	timer.start();
#endif

	// Get a list of documents labeled with this source label
	m_index.listDocumentsWithLabel(sourceLabel, docIdList); 

	// This is the labels we'll apply to new documents
	snprintf(sourceStr, 64, "SOURCE%u", m_sourceId);
	labels.insert(sourceStr);
	labels.insert(sourceLabel);

	// Parse the mbox file
	while (pFilter->next_document() == true)
	{
		Document currentMessage;

		FilterUtils::populateDocument(currentMessage, pFilter);

		// Has this message already been indexed ?
		unsigned int docId = m_index.hasDocument(currentMessage.getLocation());
		if (docId == 0)
		{
			m_index.setStemmingMode(IndexInterface::STORE_BOTH);

			unsigned int docId = 0;
			indexedFile = FilterWrapper::indexDocument(m_index, currentMessage, labels, docId);
#ifdef DEBUG
			if (indexedFile == false)
			{
				cout << "MboxHandler::parseMailAccount: couldn't index message " << docNum << endl;
			}
#endif
		}
		else
		{
#ifdef DEBUG
			cout << "MboxHandler::parseMailAccount: already indexed message "
				<< docNum << ", document ID " << docId << endl;
#endif
			set<unsigned int>::iterator docIter = docIdList.find(docId);
			if (docIter != docIdList.end())
			{
				// Remove this document from the list
				docIdList.erase(docIter);
			}
		}

		++docNum;
	}
#ifdef DEBUG
	cout << "MboxHandler::parseMailAccount: parsed " << docNum << " documents in "
		<< timer.stop()/1000 << " ms" << endl;
#endif

	// Any document still in the list wasn't found this time around
	// and should be unindexed
	deleteMessages(docIdList);

	return indexedFile;
}

bool MboxHandler::deleteMessages(set<unsigned int> &docIdList)
{
	bool unindexedMsgs = false;

#ifdef DEBUG
	cout << "MboxHandler::deleteMessages: " << docIdList.size() << " message(s) to unindex" << endl;
#endif
	for (set<unsigned int>::iterator docIter = docIdList.begin();
		docIter != docIdList.end(); ++docIter)
	{
#ifdef DEBUG
		cout << "MboxHandler::deleteMessages: unindexing document ID " << *docIter << endl;
#endif
		if (m_index.unindexDocument(*docIter) == true)
		{
			unindexedMsgs = true;
		}
	}

	return unindexedMsgs;
}

void MboxHandler::initialize(void)
{
	set<string> mailboxes;

	// Get the mail accounts map
	set<PinotSettings::TimestampedItem> &mailAccounts = PinotSettings::getInstance().m_mailAccounts;
	for (set<PinotSettings::TimestampedItem>::iterator mailIter = mailAccounts.begin();
		mailIter != mailAccounts.end(); ++mailIter)
	{
		m_fileNames.insert(mailIter->m_name);
	}

	// Unindex messages that belong to mailboxes that no longer exist
	if (m_history.getSourceItems(m_sourceId, CrawlHistory::CRAWLED, mailboxes) > 0)
	{
		for(set<string>::const_iterator mailIter = mailboxes.begin();
			mailIter != mailboxes.end(); ++mailIter)
		{
			Url urlObj(*mailIter);

			// Is this a file and does it still exist ?
			if ((urlObj.getProtocol() == "file") &&
				(m_fileNames.find(mailIter->substr(7)) == m_fileNames.end()))
			{
				string sourceLabel("mailbox://");

				sourceLabel += urlObj.getLocation();
				sourceLabel += "/";
				sourceLabel += urlObj.getFile();

#ifdef DEBUG
				cout << "MboxHandler::initialize: removing messages with label "
					<< sourceLabel << endl;
#endif
				// All documents with this label will be unindexed
				m_index.unindexDocuments(sourceLabel, false);

				// Delete this item
				m_history.deleteItem(*mailIter);
			}
#ifdef DEBUG
			else cout << "MboxHandler::initialize: " << *mailIter
				<< " still configured for monitoring" << endl;
#endif
		}
	}
#ifdef DEBUG
	cout << "MboxHandler::initialize: " << m_fileNames.size() << " mail accounts" << endl;
#endif
}

void MboxHandler::flushIndex(void)
{
	// The index is flushed after indexing a mailbox
}

bool MboxHandler::fileExists(const string &fileName)
{
	PinotSettings::TimestampedItem mailAccount;

#ifdef DEBUG
	cout << "MboxHandler::fileExists: " << fileName << endl;
#endif
	if (checkMailAccount(fileName, mailAccount) == false)
	{
		return false;
	}

	return indexMessages(fileName, mailAccount, 0);
}

bool MboxHandler::fileCreated(const string &fileName)
{
	// Nothing to do here
	return true;
}

bool MboxHandler::fileModified(const string &fileName)
{
	PinotSettings::TimestampedItem mailAccount;

#ifdef DEBUG
	cout << "MboxHandler::fileModified: " << fileName << " changed" << endl;
#endif
	if (checkMailAccount(fileName, mailAccount) == false)
	{
		return false;
	}

	// Parse the file in whole
	return indexMessages(fileName, mailAccount, 0);
}

bool MboxHandler::fileMoved(const string &fileName, const string &previousFileName)
{
	// Nothing to do here
	return true;
}

bool MboxHandler::directoryMoved(const string &dirName,
	const string &previousDirName)
{
	// Nothing to do here
	return true;
}

bool MboxHandler::fileDeleted(const string &fileName)
{
	set<unsigned int> docIdList;
	string sourceLabel("mailbox://");

	sourceLabel += fileName;

	if (m_index.isGood() == false)
	{
		cerr << "MboxHandler::fileDeleted: couldn't get mail index" << endl;
		return false;
	}

	// Get a list of documents labeled with this source label
	if (m_index.listDocumentsWithLabel(sourceLabel, docIdList) == true)
	{
		// Unindex all documents labeled with this source label
		deleteMessages(docIdList); 
		// Delete the label
		m_index.deleteLabel(sourceLabel);

		return true;
	}

	return false;
}

bool MboxHandler::directoryDeleted(const string &dirName)
{
	// Nothing to do here
	return true;
}

