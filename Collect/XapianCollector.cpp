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
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <iostream>

#include "IndexedDocument.h"
#include "StringManip.h"
#include "Url.h"
#include "XapianCollector.h"

using namespace std;

XapianCollector::XapianCollector() :
	DownloaderInterface()
{
	m_databaseName = "";
	// Always get documents data from the index
	m_getDocData = true;
	m_pDatabase = NULL;
}

XapianCollector::XapianCollector(const string &database, bool getDocData) :
	DownloaderInterface()
{
	m_databaseName = database;
	m_getDocData = getDocData;
	m_pDatabase = NULL;

	// Try opening this database
	if (openDatabase() == false)
	{
		m_databaseName.clear();
		m_pDatabase = NULL;
	}
	m_getDocData = getDocData;
}

XapianCollector::~XapianCollector()
{
	if (m_pDatabase != NULL)
	{
		delete m_pDatabase;
	}
}

bool XapianCollector::openDatabase(void)
{
	struct stat dbStat;

	if (m_databaseName.empty() == true)
	{
		return false;
	}

	// The specified path must be either a directory or a symlink (to a directory)
	if (stat(m_databaseName.c_str(), &dbStat) == -1)
	{
		// Database directory doesn't exist
		cerr << "XapianCollector::openDatabase: " << m_databaseName << " doesn't exist" << endl;
		return false;
	}
	else if (!S_ISDIR(dbStat.st_mode))
	{
		cerr << "XapianCollector::openDatabase: " << m_databaseName << " is not a directory" << endl;
		return false;
	}

	// Try opening it now
	try
	{
#ifdef DEBUG
		cout << "XapianCollector::openDatabase: opening database " << m_databaseName << endl;
#endif
		m_pDatabase = new Xapian::Database(m_databaseName);

		return true;
	}
	catch (const Xapian::Error &error)
	{
		cerr << "XapianCollector::openDatabase: couldn't open database: " << error.get_msg() << endl;
	}

	return false;
}

//
// Implementation of DownloaderInterface
//

/// Retrieves the specified document; NULL if error.
Document *XapianCollector::retrieveUrl(const DocumentInfo &docInfo)
{
	string url = docInfo.getLocation();
	Url thisUrl(url);

	if (thisUrl.getProtocol() != "xapian")
	{
		// We can't handle that type of protocol...
		return NULL;
	}

	// Ignore host portion of the URL as we can only deal with local indexes
	// Get the database location and document ID out of the location field
	string database = thisUrl.getLocation();
	string documentId = thisUrl.getFile();
	Xapian::docid docId;
	sscanf(documentId.c_str(), "%u", &docId);
#ifdef DEBUG
	cout << "XapianCollector::retrieveUrl: database is " << database << ", document ID is " << docId << endl;
#endif

	// The constructor may already have opened an index, check this URL corresponds
	if (database != m_databaseName)
	{
		// The requested URL is in some other index
		if (m_pDatabase != NULL)
		{
			delete m_pDatabase;
			m_pDatabase = NULL;
		}
		// Try opening that index then
		m_databaseName = database;
		if (openDatabase() == false)
		{
			m_pDatabase = NULL;
			m_databaseName.clear();
		}
	}

	if (m_pDatabase == NULL)
	{
		return NULL;
	}

	IndexedDocument *pIndexedDoc = NULL;

	try
	{
		// Now retrieve the desired document
		Xapian::Document doc = m_pDatabase->get_document(docId);
		// ... and its data
		string record = doc.get_data();

		// Extract the title, location, summary, type and timestamp
		string title = StringManip::extractField(record, "caption=", "\n");
#ifdef DEBUG
		cout << "XapianCollector::retrieveUrl: found omindex title " << title << endl;
#endif
		string location = StringManip::extractField(record, "url=", "\n");
		string type = StringManip::extractField(record, "type=", "\n");
		string timestamp = StringManip::extractField(record, "timestamp=", "\n");
		string language = StringManip::extractField(record, "language=", "\n");
#ifdef DEBUG
		cout << "XapianCollector::retrieveUrl: " << docId << " was indexed from " << location << " at " << timestamp << endl;
#endif

		pIndexedDoc = new IndexedDocument(title, url, location, type, language);
		pIndexedDoc->setTimestamp(timestamp);

		// Extract document's data ?
		if (m_getDocData == true)
		{
			// The only data we have at hand is the summary
			string summary = StringManip::extractField(record, "sample=", "\n");
#ifdef DEBUG
			cout << "XapianCollector::retrieveUrl: found omindex summary " << summary << endl;
#endif
			pIndexedDoc->setData(summary.c_str(), summary.length());
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't retrieve document: " << error.get_msg() << endl;
	}

	return pIndexedDoc;
}
