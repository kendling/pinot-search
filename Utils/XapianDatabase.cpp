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
#include <stdio.h>
#include <iostream>

#include "XapianDatabase.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;

XapianDatabase::XapianDatabase(const string &databaseName, bool readOnly) :
	m_databaseName(databaseName),
	m_readOnly(readOnly),
	m_pDatabase(NULL),
	m_isOpen(false),
	m_merge(false),
	m_pFirst(NULL),
	m_pSecond(NULL)
{
	pthread_rwlock_init(&m_rwLock, NULL);
	openDatabase();
}

XapianDatabase::XapianDatabase(const string &databaseName, 
	XapianDatabase *pFirst, XapianDatabase *pSecond) :
	m_databaseName(databaseName),
	m_readOnly(true),
	m_pDatabase(NULL),
	m_isOpen(pFirst->m_isOpen),
	m_merge(true),
	m_pFirst(pFirst),
	m_pSecond(pSecond)
{
	pthread_rwlock_init(&m_rwLock, NULL);
}

XapianDatabase::XapianDatabase(const XapianDatabase &other) :
	m_databaseName(other.m_databaseName),
	m_readOnly(other.m_readOnly),
	m_pDatabase(NULL),
	m_isOpen(other.m_isOpen),
	m_merge(other.m_merge),
	m_pFirst(other.m_pFirst),
	m_pSecond(other.m_pSecond)
{
	pthread_rwlock_init(&m_rwLock, NULL);
	if (other.m_pDatabase != NULL)
	{
		m_pDatabase = new Xapian::Database(*other.m_pDatabase);
#ifdef DEBUG
		cout << "XapianDatabase: copied " << m_databaseName << endl;
#endif
	}
}

XapianDatabase::~XapianDatabase()
{
	if (m_pDatabase != NULL)
	{
		delete m_pDatabase;
#ifdef DEBUG
		cout << "XapianDatabase::~XapianDatabase:unlock: deleted" << endl;
#endif
	}
	pthread_rwlock_destroy(&m_rwLock);
}

XapianDatabase &XapianDatabase::operator=(const XapianDatabase &other)
{
	m_databaseName = other.m_databaseName;
	m_readOnly = other.m_readOnly;
	if (m_pDatabase != NULL)
	{
		delete m_pDatabase;
		m_pDatabase = NULL;
#ifdef DEBUG
		cout << "XapianDatabase::operator=: deleted" << endl;
#endif
	}
	if (other.m_pDatabase != NULL)
	{
		m_pDatabase = new Xapian::Database(*other.m_pDatabase);
#ifdef DEBUG
		cout << "XapianDatabase::operator=: copied " << m_databaseName << endl;
#endif
	}
	m_isOpen = other.m_isOpen;
	m_merge = other.m_merge;
	m_pFirst = other.m_pFirst;
	m_pSecond = other.m_pSecond;

	return *this;
}

void XapianDatabase::openDatabase(void)
{
	struct stat dbStat;

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
#ifdef DEBUG
		cout << "XapianDatabase::openDatabase: deleted" << endl;
#endif
	}

	// Is it a remote database ?
	string::size_type slashPos = m_databaseName.find("/");
	string::size_type colonPos = m_databaseName.find(":");
	if ((slashPos == string::npos) &&
		(colonPos != string::npos))
	{
		string hostName = m_databaseName.substr(0, colonPos);
		unsigned int port = (unsigned int)atoi(m_databaseName.substr(colonPos + 1).c_str());

		if (m_readOnly == false)
		{
			cerr << "XapianDatabase::openDatabase: remote databases are read-only" << endl;
			return;
		}

		try
		{
#ifdef DEBUG
			cout << "XapianDatabase::openDatabase: remote database at "
				<< hostName << ":" << port << endl;
#endif
			Xapian::Database remoteDatabase = Xapian::Remote::open(hostName, port);
			m_pDatabase = new Xapian::Database(remoteDatabase);
			m_isOpen = true;

			return;
		}
		catch (const Xapian::Error &error)
		{
			cerr << "XapianDatabase::openDatabase: couldn't open remote database: "
				<< error.get_msg() << endl;
		}

		return;		
	}

	// It's a local database : the specified path must be a directory
	if (stat(m_databaseName.c_str(), &dbStat) == -1)
	{
		if (m_readOnly == true)
		{
			cerr << "XapianDatabase::openDatabase: database " << m_databaseName
				<< " doesn't exist" << endl;
			return;
		}

		// Database directory doesn't exist, create it (mode 755)
		if (mkdir(m_databaseName.c_str(), (mode_t)S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) != 0)
		{
			cerr << "XapianDatabase::openDatabase: couldn't create database directory "
				<< m_databaseName << endl;
			return;
		}
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
			m_pDatabase = new Xapian::Database(m_databaseName);
		}
		else
		{
			m_pDatabase = new Xapian::WritableDatabase(m_databaseName, Xapian::DB_CREATE_OR_OPEN);
		}
#ifdef DEBUG
		cout << "XapianDatabase::openDatabase: opened " << m_databaseName << endl;
#endif
		m_isOpen = true;

		return;
	}
	catch (const Xapian::Error &error)
	{
		cerr << "XapianDatabase::openDatabase: couldn't open database: "
			<< error.get_msg() << endl;
	}

	return;
}

/// Returns false if the database couldn't be opened.
bool XapianDatabase::isOpen(void) const
{
	return m_isOpen;
}

/// Attempts to lock and retrieve the database.
Xapian::Database *XapianDatabase::readLock(void)
{
	if (m_merge == false)
	{
#ifdef DEBUG
		cout << "XapianDatabase::readLock: " << m_databaseName << endl;
#endif
		if (pthread_rwlock_rdlock(&m_rwLock) == 0)
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

		if (pthread_rwlock_rdlock(&m_rwLock) == 0)
		{
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
#ifdef DEBUG
			cout << "XapianDatabase::readLock: copied " << m_pFirst->m_databaseName << endl;
#endif

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
		// FIXME: close and reopen in write mode
		cerr << "Couldn't open read-only database " << m_databaseName
			<< " for writing" << endl;
		return NULL;
	}

#ifdef DEBUG
	cout << "XapianDatabase::writeLock: " << m_databaseName << endl;
#endif
	if (pthread_rwlock_wrlock(&m_rwLock) == 0)
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
#ifdef DEBUG
	cout << "XapianDatabase::unlock: " << m_databaseName << endl;
#endif
	pthread_rwlock_unlock(&m_rwLock);

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
#ifdef DEBUG
			cout << "XapianDatabase::unlock: deleted merge" << endl;
#endif
		}
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
