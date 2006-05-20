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
#include "TokenizerFactory.h"
#include "FileCollector.h"
#include "XapianIndex.h"
#include "XapianEngine.h"
#include "PinotUtils.h"
#include "MonitorHandler.h"

using namespace std;
using namespace SigC;

MonitorHandler::MonitorHandler()
{
}

MonitorHandler::~MonitorHandler()
{
}

Signal3<void, IndexedDocument, unsigned int, string>& MonitorHandler::getUpdateSignal(void)
{
	return m_signalUpdate;
}

MboxHandler::MboxHandler() :
	MonitorHandler(),
	m_locationsCount(0)
{
}

MboxHandler::~MboxHandler()
{
}

bool MboxHandler::checkMailAccount(const string &fileName, PinotSettings::MailAccount &mailAccount,
		off_t &previousSize)
{
	struct stat fileStat;

	mailAccount.m_name = to_utf8(fileName);

	// Ensure it's one of our mail accounts
	set<PinotSettings::MailAccount> &mailAccounts = PinotSettings::getInstance().m_mailAccounts;
	set<PinotSettings::MailAccount>::iterator mailIter = mailAccounts.find(mailAccount);
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

	if (fileStat.st_mtime <= mailIter->m_modTime)
	{
		// No change since last time...
#ifdef DEBUG
		cout << "MboxHandler::checkMailAccount: not modified since last time ("
			<< mailIter->m_modTime << ">" << fileStat.st_mtime << ")" << endl;
#endif
		return false;
	}
#ifdef DEBUG
	cout << "MboxHandler::checkMailAccount: modified since last time ("
		<< mailIter->m_modTime << "<" << fileStat.st_mtime << ")" << endl;
#endif

	// Update this mail account's properties
	mailAccount = (*mailIter);
	mailAccount.m_modTime = fileStat.st_mtime;
	previousSize = mailAccount.m_size;
	mailAccount.m_size = fileStat.st_size;

	return true;
}

bool MboxHandler::parseMailAccount(MboxParser &boxParser, IndexInterface *pIndex,
	time_t &lastMessageTime,
	const string &tempSourceLabel, const string &sourceLabel)
{
	bool indexedFile = false;

	if (pIndex == NULL)
	{
		return false;
	}

	// Parse the mbox file
#ifdef DEBUG
	Timer timer;
	timer.start();
#endif
	const Document *pMessage = boxParser.getDocument();
	unsigned int docNum = 0;

	while (pMessage != NULL)
	{
		// Has this message already been indexed ?
		unsigned int docId = pIndex->hasDocument(pMessage->getLocation());
		if (docId == 0)
		{
			pIndex->setStemmingMode(IndexInterface::STORE_BOTH);

			// Get an ad hoc tokenizer for the message
			Tokenizer *pTokenizer = TokenizerFactory::getTokenizerByType(pMessage->getType(), pMessage);
			if (pTokenizer == NULL)
			{
#ifdef DEBUG
				cout << "MboxHandler::parseMailAccount: no tokenizer for message " << docNum << endl;
#endif
				break;	
			}

			set<string> labels;
			labels.insert(tempSourceLabel);

			unsigned int docId = 0;
			indexedFile = pIndex->indexDocument(*pTokenizer, labels, docId);
			if (indexedFile == true)
			{
				time_t messageDate = boxParser.getDate();

				if (messageDate > lastMessageTime)
				{
					// This is the latest message so far
					lastMessageTime = messageDate;
				}

				pIndex->setDocumentLabels(docId, labels);

				IndexedDocument docInfo(pMessage->getTitle(),
					XapianEngine::buildUrl(PinotSettings::getInstance().m_mailIndexLocation, docId),
					pMessage->getLocation(), pMessage->getType(), pMessage->getLanguage());
				docInfo.setTimestamp(TimeConverter::toTimestamp(messageDate));

				// Signal
				m_signalUpdate(docInfo, docId, _("My Email"));
			}
#ifdef DEBUG
			else cout << "MboxHandler::parseMailAccount: couldn't index message " << docNum << endl;
#endif

			delete pTokenizer;
		}
		else
		{
#ifdef DEBUG
			cout << "MboxHandler::parseMailAccount: already indexed message "
				<< docNum << ", document ID " << docId << endl;
#endif
			if (sourceLabel.empty() == false)
			{
				set<string> labels;

				// Get the message's labels
				pIndex->getDocumentLabels(docId, labels);
				// The source label must have been applied to the message when originally indexed
				set<string>::iterator labelIter = labels.find(sourceLabel.c_str());
				if (labelIter != labels.end())
				{
					// Erase it
					labels.erase(labelIter);
					// Add the temporary label
					labels.insert(tempSourceLabel);
					pIndex->setDocumentLabels(docId, labels);
				}
			}
		}

		// More messages ?
		if (boxParser.nextMessage() == false)
		{
#ifdef DEBUG
			cout << "MboxHandler::parseMailAccount: no more messages from parser" << endl;
#endif
			break;
		}
		pMessage = boxParser.getDocument();
		++docNum;
	}
#ifdef DEBUG
	long microsecs = timer.stop();
	cout << "MboxHandler::parseMailAccount: parsed " << docNum << " documents in "
		<< microsecs/1000000 << " seconds (" << microsecs << ")" << endl;
#endif

	return indexedFile;
}

bool MboxHandler::deleteMessages(IndexInterface *pIndex, const string &sourceLabel)
{
	set<unsigned int> docIdList;
	bool unindexedMsgs = false;

	if (pIndex == NULL)
	{
		return false;
	}

	// Unindex all documents labeled with this source label
	if ((pIndex->listDocumentsWithLabel(sourceLabel, docIdList) == true) &&
		(docIdList.empty() == false))
	{
#ifdef DEBUG
		cout << "MboxHandler::deleteMessages: " << docIdList.size() << " message(s) to unindex" << endl;
#endif
		for (set<unsigned int>::iterator docIter = docIdList.begin();
			docIter != docIdList.end(); ++docIter)
		{
#ifdef DEBUG
			cout << "MboxHandler::deleteMessages: unindexing document ID " << *docIter << endl;
#endif
			if (pIndex->unindexDocument(*docIter) == true)
			{
				unindexedMsgs = true;
			}
		}
	}

	return unindexedMsgs;
}

bool MboxHandler::getLocations(set<string> &newLocations,
	set<string> &locationsToRemove)
{
	newLocations.clear();
	locationsToRemove.clear();

	copy(m_locations.begin(), m_locations.end(),
		inserter(locationsToRemove, locationsToRemove.begin()));

	// Get the mail accounts map
	set<PinotSettings::MailAccount> &mailAccounts = PinotSettings::getInstance().m_mailAccounts;
	for (set<PinotSettings::MailAccount>::iterator mailIter = mailAccounts.begin();
		mailIter != mailAccounts.end(); ++mailIter)
	{
		// Is this a known location ?
		set<string>::iterator locationIter = m_locations.find(mailIter->m_name);
		if (locationIter == m_locations.end())
		{
			// No, it is new
			m_locations.insert(mailIter->m_name);
			newLocations.insert(mailIter->m_name);
		}
		else
		{
			// Since it's a known location, we'd better not remove it
			set<string>::iterator removeIter = locationsToRemove.find(mailIter->m_name);
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
	cout << "MboxHandler::getLocations: " << m_locations.size() << " locations, "
		<< newLocations.size() << " new, " << locationsToRemove.size() << " to be removed" << endl;
#endif

	if ((newLocations.empty() == false) ||
		(locationsToRemove.empty() == false))
	{
		return true;
	}

	return false;
}

bool MboxHandler::fileExists(const string &fileName)
{
	PinotSettings::MailAccount mailAccount;
	off_t previousSize = 0;

#ifdef DEBUG
	cout << "MboxHandler::fileExists: " << fileName << endl;
#endif
	if (checkMailAccount(fileName, mailAccount, previousSize) == false)
	{
		return false;
	}

	// Come up with a label for this mbox file's messages
	string sourceLabel = "mailbox://";
	sourceLabel += fileName;
	string tempSourceLabel = "Temp";
	tempSourceLabel += sourceLabel;

	// Get the mail index
	XapianIndex index(PinotSettings::getInstance().m_mailIndexLocation);
	if (index.isGood() == false)
	{
		cerr << "MboxHandler::fileExists: couldn't get mail index" << endl;
		return false;
	}

	// Get a parser
	MboxParser boxParser(fileName);

	bool indexedFile = parseMailAccount(boxParser, &index, 
		mailAccount.m_lastMessageTime, tempSourceLabel, sourceLabel);

	// Any document still labeled with this source label wasn't found
	// this time around and should be unindexed
	if (deleteMessages(&index, sourceLabel) == true)
	{
		indexedFile = true;
	}

	// Rename the temporary label for next time the mbox is parsed
	index.deleteLabel(sourceLabel);
	index.renameLabel(tempSourceLabel, sourceLabel);

	// Flush the index
	index.flush();

	// Update this mail account in the list
	set<PinotSettings::MailAccount> &mailAccounts = PinotSettings::getInstance().m_mailAccounts;
	set<PinotSettings::MailAccount>::iterator mailIter = mailAccounts.find(mailAccount);
	if (mailIter != mailAccounts.end())
	{
		mailAccounts.erase(mailIter);
	}
	mailAccounts.insert(mailAccount);

	return indexedFile;
}

bool MboxHandler::fileCreated(const string &fileName)
{
	// Nothing to do here
	return true;
}

bool MboxHandler::fileModified(const string &fileName)
{
	PinotSettings::MailAccount mailAccount;
	off_t previousSize = 0, mboxOffset = 0;

#ifdef DEBUG
	cout << "MboxHandler::fileModified: " << fileName << " changed" << endl;
#endif
	if (checkMailAccount(fileName, mailAccount, previousSize) == false)
	{
		return false;
	}

	if (mailAccount.m_size <= previousSize)
	{
		// Parse the file from the beginning...
#ifdef DEBUG
		cout << "MboxHandler::fileModified: file smaller or same size" << endl;
#endif
		return fileExists(fileName);
	}
#ifdef DEBUG
	else cout << "MboxHandler::fileModified: file now larger than " << previousSize << endl;
#endif

	// Chances are new messages were added but none removed
	mboxOffset = previousSize;

	// Come up with a label for this mbox file's messages
	string sourceLabel = "mailbox://";
	sourceLabel += fileName;

	// Get the mail index
	XapianIndex index(PinotSettings::getInstance().m_mailIndexLocation);
	if (index.isGood() == false)
	{
		cerr << "MboxHandler::fileModified: couldn't get mail index" << endl;
		return false;
	}

	// Get a parser
	MboxParser boxParser(fileName, mboxOffset);

	bool indexedFile = parseMailAccount(boxParser, &index,
		mailAccount.m_lastMessageTime, sourceLabel, "");
	if (indexedFile == true)
	{
		// Do not attempt to find out if some of the messages were removed
		// Some messages may also be indexed twice now, eg if another message
		// was inserted and changed offsets
		// Let the next fileExists() deal with it and clean up the whole thing

		// Flush the index
		index.flush();
	}

	// Update this mail account in the list
	set<PinotSettings::MailAccount> &mailAccounts = PinotSettings::getInstance().m_mailAccounts;
	set<PinotSettings::MailAccount>::iterator mailIter = mailAccounts.find(mailAccount);
	if (mailIter != mailAccounts.end())
	{
		mailAccounts.erase(mailIter);
	}
	mailAccounts.insert(mailAccount);

	return indexedFile;
}

bool MboxHandler::fileMoved(const string &fileName)
{
	// Nothing to do here
	return true;
}

bool MboxHandler::fileDeleted(const string &fileName)
{
	string sourceLabel = string("mailbox://") + fileName;
	bool unindexedFile = false;

	// Get the mail index
	XapianIndex index(PinotSettings::getInstance().m_mailIndexLocation);
	if (index.isGood() == false)
	{
		cerr << "MboxHandler::fileDeleted: couldn't get mail index" << endl;
		return false;
	}

	// Unindex all documents labeled with this source label
	if (deleteMessages(&index, sourceLabel) == true)
	{
		// Delete the label
		index.deleteLabel(sourceLabel);

		return true;
	}

	return false;
}
