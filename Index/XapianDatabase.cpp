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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <regex.h>
#include <stdio.h>
#include <iostream>

#include "StringManip.h"
#include "TimeConverter.h"
#include "Url.h"
#include "XapianDatabase.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;

// This puts a limit to terms length.
const unsigned int XapianDatabase::m_maxTermLength = 230;

XapianDatabase::XapianDatabase(const string &databaseName,
	bool readOnly, bool overwrite) :
	m_databaseName(databaseName),
	m_readOnly(readOnly),
	m_overwrite(overwrite),
	m_obsoleteFormat(false),
	m_pDatabase(NULL),
	m_isOpen(false),
	m_merge(false),
	m_pFirst(NULL),
	m_pSecond(NULL)
{
	pthread_mutex_init(&m_rwLock, NULL);
	openDatabase();
}

XapianDatabase::XapianDatabase(const string &databaseName, 
	XapianDatabase *pFirst, XapianDatabase *pSecond) :
	m_databaseName(databaseName),
	m_readOnly(true),
	m_overwrite(false),
	m_obsoleteFormat(false),
	m_pDatabase(NULL),
	m_isOpen(pFirst->m_isOpen),
	m_merge(true),
	m_pFirst(pFirst),
	m_pSecond(pSecond)
{
	pthread_mutex_init(&m_rwLock, NULL);
}

XapianDatabase::XapianDatabase(const XapianDatabase &other) :
	m_databaseName(other.m_databaseName),
	m_readOnly(other.m_readOnly),
	m_overwrite(other.m_overwrite),
	m_obsoleteFormat(other.m_obsoleteFormat),
	m_pDatabase(NULL),
	m_isOpen(other.m_isOpen),
	m_merge(other.m_merge),
	m_pFirst(other.m_pFirst),
	m_pSecond(other.m_pSecond)
{
	pthread_mutex_init(&m_rwLock, NULL);
	if (other.m_pDatabase != NULL)
	{
		m_pDatabase = new Xapian::Database(*other.m_pDatabase);
	}
}

XapianDatabase::~XapianDatabase()
{
	if (m_pDatabase != NULL)
	{
		delete m_pDatabase;
	}
	pthread_mutex_destroy(&m_rwLock);
}

XapianDatabase &XapianDatabase::operator=(const XapianDatabase &other)
{
	if (this != &other)
	{
		m_databaseName = other.m_databaseName;
		m_readOnly = other.m_readOnly;
		m_overwrite = other.m_overwrite;
		m_obsoleteFormat = other.m_obsoleteFormat;
		if (m_pDatabase != NULL)
		{
			delete m_pDatabase;
			m_pDatabase = NULL;
		}
		if (other.m_pDatabase != NULL)
		{
			m_pDatabase = new Xapian::Database(*other.m_pDatabase);
		}
		m_isOpen = other.m_isOpen;
		m_merge = other.m_merge;
		m_pFirst = other.m_pFirst;
		m_pSecond = other.m_pSecond;
	}

	return *this;
}

void XapianDatabase::openDatabase(void)
{
	struct stat dbStat;
	bool createDatabase = false;
	bool tryAgain = false;

	if (m_databaseName.empty() == true)
	{
		return;
	}

	// Assume things will fail
	m_isOpen = false;

	if (m_pDatabase != NULL)
	{
		delete m_pDatabase;
		m_pDatabase = NULL;
	}

	// Is it a remote database ?
	string::size_type slashPos = m_databaseName.find("/");
	string::size_type colonPos = m_databaseName.find(":");
	if (((slashPos == string::npos) ||
		(slashPos > 0)) &&
		(colonPos != string::npos))
	{
		Url urlObj(m_databaseName);

		if (m_readOnly == false)
		{
			cerr << "XapianDatabase::openDatabase: remote databases " << m_databaseName << " are read-only" << endl;
			return;
		}

		if (m_databaseName.find("://") == string::npos)
		{
			// It's an old style remote specification without the protocol
			urlObj = Url("tcpsrv://" + m_databaseName);
		}

		string hostName(urlObj.getHost());
		// A port number should be included
		colonPos = hostName.find(":");
		if (colonPos != string::npos)
		{
			string protocol(urlObj.getProtocol());
			string portStr(hostName.substr(colonPos + 1));
			unsigned int port = (unsigned int)atoi(portStr.c_str());

			hostName.resize(colonPos);
			try
			{
				if (protocol == "progsrv+ssh")
				{
					string args("-p");

					args += " ";
					args += portStr;
					args += " -f ";
					args += hostName;
					args += " xapian-progsrv /";
					args += urlObj.getLocation();
					args += "/";
					args += urlObj.getFile();
#ifdef DEBUG
					cout << "XapianDatabase::openDatabase: remote ssh access with ssh "
						<< args << endl;
#endif
					Xapian::Database remoteDatabase = Xapian::Remote::open("ssh", args);
					m_pDatabase = new Xapian::Database(remoteDatabase);
				}
				else
				{
#ifdef DEBUG
					cout << "XapianDatabase::openDatabase: remote database at "
						<< hostName << " " << port << endl;
#endif
					Xapian::Database remoteDatabase = Xapian::Remote::open(hostName, port);
					m_pDatabase = new Xapian::Database(remoteDatabase);
					m_isOpen = true;
				}

				return;
			}
			catch (const Xapian::Error &error)
			{
				cerr << "XapianDatabase::openDatabase: " << error.get_type()
					<< ": " << error.get_msg() << endl;
			}
		}
#ifdef DEBUG
		else cout << "XapianDatabase::openDatabase: invalid remote database at "
			<< hostName << "/" << urlObj.getLocation() << "/" << urlObj.getFile() << endl;
#endif

		return;
	}

	// It's a local database : the specified path must be a directory
	if (stat(m_databaseName.c_str(), &dbStat) == -1)
	{
#ifdef DEBUG
		cout << "XapianDatabase::openDatabase: database " << m_databaseName
			<< " doesn't exist" << endl;
#endif

		// Database directory doesn't exist, create it (mode 755)
		if (mkdir(m_databaseName.c_str(), (mode_t)S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) != 0)
		{
			cerr << "XapianDatabase::openDatabase: couldn't create database directory "
				<< m_databaseName << endl;
			return;
		}
		createDatabase = true;
	}
	else if (!S_ISDIR(dbStat.st_mode))
	{
		cerr << "XapianDatabase::openDatabase: " << m_databaseName
			<< " is not a directory" << endl;
		return;
	}

	// Try opening it now, creating if if necessary
	try
	{
		if (m_readOnly == true)
		{
			if (createDatabase == true)
			{
				// We have to create the whole thing in read-write mode first
				Xapian::WritableDatabase *pTmpDatabase = new Xapian::WritableDatabase(m_databaseName, Xapian::DB_CREATE_OR_OPEN);
				// ...then close and open again in read-only mode
				delete pTmpDatabase;
			}

			m_pDatabase = new Xapian::Database(m_databaseName);
		}
		else
		{
			int openAction = Xapian::DB_CREATE_OR_OPEN;

			if (m_overwrite == true)
			{
				// An existing database will be overwritten
				openAction = Xapian::DB_CREATE_OR_OVERWRITE;
			}

			m_pDatabase = new Xapian::WritableDatabase(m_databaseName, openAction);
		}
#ifdef DEBUG
		cout << "XapianDatabase::openDatabase: opened " << m_databaseName << endl;
#endif
		m_isOpen = true;

		return;
	}
#if XAPIAN_MAJOR_VERSION>0
	catch (const Xapian::DatabaseVersionError &error)
	{
		cerr << "XapianDatabase::openDatabase: " << error.get_type()
			<< ": " << error.get_msg() << endl;

		// This format is no longer supported
		if (m_obsoleteFormat == false)
		{
			tryAgain = true;
		}
	}
#endif
	catch (const Xapian::Error &error)
	{
		cerr << "XapianDatabase::openDatabase: " << error.get_type()
			<< ": " << error.get_msg() << endl;
	}

	// Give it another try ?
	if (tryAgain == true)
	{
		cout << "XapianDatabase::openDatabase: trying again" << endl;

		m_overwrite = true;
		m_obsoleteFormat = true;
		openDatabase();
	}
}

/// Returns false if the database couldn't be opened.
bool XapianDatabase::isOpen(void) const
{
	return m_isOpen;
}

/// Returns false if the database was of an obsolete format.
bool XapianDatabase::wasObsoleteFormat(void) const
{
	return m_obsoleteFormat;
}

/// Reopens the database.
void XapianDatabase::reopen(void)
{
	// This is provided by Xapian::Database
	// FIXME: get the write lock to make sure read operations are not in progress ?
	if (pthread_mutex_lock(&m_rwLock) == 0)
	{
		if (m_pDatabase != NULL)
		{
			m_pDatabase->reopen();
		}

		pthread_mutex_unlock(&m_rwLock);
	}
}

/// Attempts to lock and retrieve the database.
Xapian::Database *XapianDatabase::readLock(void)
{
	if (m_merge == false)
	{
		if (pthread_mutex_lock(&m_rwLock) == 0)
		{
			if (m_pDatabase == NULL)
			{
				// Try again
				openDatabase();
			}
			return m_pDatabase;
		}
	}
	else
	{
		if ((m_pFirst == NULL) ||
			(m_pFirst->isOpen() == false) ||
			(m_pSecond == NULL) ||
			(m_pSecond->isOpen() == false))
		{
			return NULL;
		}

		if (pthread_mutex_lock(&m_rwLock) == 0)
		{
			// Reopen the second index
			m_pSecond->reopen();

			// Lock both indexes
			Xapian::Database *pFirstDatabase = m_pFirst->readLock();
			Xapian::Database *pSecondDatabase = m_pSecond->readLock();
			// Copy the first one
			m_pDatabase = new Xapian::Database(*pFirstDatabase);
			// Add the second index to it
			if (pSecondDatabase != NULL)
			{
				m_pDatabase->add_database(*pSecondDatabase);
			}
			// Until unlock() is called, both indexes are read locked

			return m_pDatabase;
		}
	}

	return NULL;
}

/// Attempts to lock and retrieve the database.
Xapian::WritableDatabase *XapianDatabase::writeLock(void)
{
	if ((m_readOnly == true) ||
		(m_merge == true))
	{
		cerr << "Couldn't open read-only database " << m_databaseName
			<< " for writing" << endl;
		return NULL;
	}

	if (pthread_mutex_lock(&m_rwLock) == 0)
	{
		if (m_pDatabase == NULL)
		{
			// Try again
			openDatabase();
		}

		return dynamic_cast<Xapian::WritableDatabase *>(m_pDatabase);
	}

	return NULL;
}

/// Unlocks the database.
void XapianDatabase::unlock(void)
{
	pthread_mutex_unlock(&m_rwLock);

	if (m_merge == true)
	{
		// Unlock the original indexes
		if (m_pFirst != NULL)
		{
			m_pFirst->unlock();
		}
		if (m_pSecond != NULL)
		{
			m_pSecond->unlock();
		}

		// Delete merge
		if (m_pDatabase != NULL)
		{
			delete m_pDatabase;
			m_pDatabase = NULL;
		}
	}
}

bool XapianDatabase::badRecordField(const string &field)
{
	regex_t fieldRegex;
	regmatch_t pFieldMatches[1];
	bool isBadField = false;

	// A bad field is one that includes one of our field delimiters
	if (regcomp(&fieldRegex,
		"(url|sample|caption|type|modtime|language|size)=",
		REG_EXTENDED|REG_ICASE) == 0)
	{
		if (regexec(&fieldRegex, field.c_str(), 1,
			pFieldMatches, REG_NOTBOL|REG_NOTEOL) == 0)
		{
			isBadField = true;
		}
	}
	regfree(&fieldRegex);

	return isBadField;
}

/// Returns a record for the document's properties.
string XapianDatabase::propsToRecord(DocumentInfo *pDoc)
{
	if (pDoc == NULL)
	{
		return "";
	}

	string record("url=");
	string title(pDoc->getTitle());
	string timestamp(pDoc->getTimestamp());
	time_t timeT = TimeConverter::fromTimestamp(timestamp);
	char tmpStr[64];

	// Set the document data omindex-style
	record += pDoc->getLocation();
	// The sample will be generated at query time
	record += "\nsample=";
	record += "\ncaption=";
	if (badRecordField(title) == true)
	{
		// Modify the title if necessary
		string::size_type pos = title.find("=");
		while (pos != string::npos)
		{
			title[pos] = ' ';
			pos = title.find("=", pos + 1);
		}
#ifdef DEBUG
		cout << "XapianDatabase::propsToRecord: modified title" << endl;
#endif
	}
	record += title;
	record += "\ntype=";
	record += pDoc->getType();
	// Append a timestamp, in a format compatible with Omega
	record += "\nmodtime=";
	snprintf(tmpStr, 64, "%ld", timeT);
	record += tmpStr;
	// ...and the language
	record += "\nlanguage=";
	record += StringManip::toLowerCase(pDoc->getLanguage());
	// ...and the file size
	record += "\nsize=";
	snprintf(tmpStr, 64, "%ld", pDoc->getSize());
	record += tmpStr;
#ifdef DEBUG
	cout << "XapianDatabase::propsToRecord: document data is " << record << endl;
#endif

	return record;
}

/// Sets the document's properties acording to the record.
void XapianDatabase::recordToProps(const string &record, DocumentInfo *pDoc)
{
	if (pDoc == NULL)
	{
		return;
	}

	// Get the title
	pDoc->setTitle(StringManip::extractField(record, "caption=", "\n"));
	// Get the URL
	string url(StringManip::extractField(record, "url=", "\n"));
	if (url.empty() == false)
	{
		url = Url::canonicalizeUrl(url);
	}
	pDoc->setLocation(url);
	// Get the type
	pDoc->setType(StringManip::extractField(record, "type=", "\n"));
	// ... the language, if available
	pDoc->setLanguage(StringManip::extractField(record, "language=", "\n"));
	// ... and the timestamp
	string timestamp, modTime(StringManip::extractField(record, "modtime=", "\n"));
	if (modTime.empty() == false)
	{
		time_t timeT = (time_t )atol(modTime.c_str());
		timestamp = TimeConverter::toTimestamp(timeT);
	}
	pDoc->setTimestamp(timestamp);
	string bytesSize(StringManip::extractField(record, "size=", ""));
	if (bytesSize.empty() == false)
	{
		pDoc->setSize((off_t)atol(bytesSize.c_str()));
	}
}

/// Returns the URL for the given document in the given index.
string XapianDatabase::buildUrl(const string &database, unsigned int docId)
{
	// Make up a pseudo URL
	char docIdStr[64];
	sprintf(docIdStr, "%u", docId);
	string url = "xapian://localhost/";
	url += database;
	url += "/";
	url += docIdStr;

	return url;
}

/// Truncates or partially hashes a term.
string XapianDatabase::limitTermLength(const string &term, bool makeUnique)
{
	if (term.length() > XapianDatabase::m_maxTermLength)
	{
		if (makeUnique == false)
		{
			// Truncate
			return term.substr(0, XapianDatabase::m_maxTermLength);
		}
		else
		{
			return StringManip::hashString(term, XapianDatabase::m_maxTermLength);
		}
	}

	return term;
}
