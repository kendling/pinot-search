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
#include <stdarg.h>
#include <strings.h>
#include <time.h>
#include <ctype.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <utility>
#include <xapian.h>

#include "Languages.h"
#include "StringManip.h"
#include "TimeConverter.h"
#include "Url.h"
#include "LanguageDetector.h"
#include "XapianDatabaseFactory.h"
#include "XapianIndex.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::set;
using std::min;

// This puts a limit to terms length.
const unsigned int XapianIndex::m_maxTermLength = 240;
const string XapianIndex::MAGIC_TERM = "X-MetaSE-Doc";

XapianIndex::XapianIndex(const string &indexName) :
	IndexInterface(),
	m_databaseName(indexName),
	m_goodIndex(false)
{
	// Open in read-only mode
	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName);
	if ((pDatabase != NULL) &&
		(pDatabase->isOpen() == true))
	{
		m_goodIndex = true;
	}
}

XapianIndex::XapianIndex(const XapianIndex &other) :
	IndexInterface(other),
	m_databaseName(other.m_databaseName),
	m_goodIndex(other .m_goodIndex)
{
}

XapianIndex::~XapianIndex()
{
}

XapianIndex &XapianIndex::operator=(const XapianIndex &other)
{
	IndexInterface::operator=(other);
	m_databaseName = other.m_databaseName;
	m_goodIndex = other .m_goodIndex;
	return *this;
}

string XapianIndex::limitTermLength(const string &term, bool makeUnique)
{
	if (term.length() > XapianIndex::m_maxTermLength)
	{
		if (makeUnique == false)
		{
			// Truncate
			return term.substr(0, XapianIndex::m_maxTermLength);
		}
		else
		{
			return StringManip::hashString(term, XapianIndex::m_maxTermLength);
		}
	}

	return term;
}

bool XapianIndex::listDocumentsWithTerm(const string &term, set<unsigned int> &docIds,
	unsigned int maxDocsCount, unsigned int startDoc) const
{
	unsigned int docCount = 0;

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return 0;
	}

	docIds.clear();
	try
	{
		Xapian::Database *pIndex = pDatabase->readLock();
		if (pIndex != NULL)
		{
#ifdef DEBUG
			cout << "XapianIndex::listDocumentsWithTerm: term " << term << endl;
#endif
			// Get a list of documents that have the term
			for (Xapian::PostingIterator postingIter = pIndex->postlist_begin(term);
				(postingIter != pIndex->postlist_end(term)) &&
					((maxDocsCount == 0) || (docIds.size() < maxDocsCount));
				++postingIter)
			{
				Xapian::docid docId = *postingIter;

				// We cannot use postingIter->skip_to() because startDoc isn't an ID
				if (docCount >= startDoc)
				{
					docIds.insert(docId);
				}
				++docCount;
			}
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't get document list: " << error.get_type() << ": " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't get document list, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return docIds.size();
}

//
// Implementation of IndexInterface
//

/// Returns false if the index couldn't be opened.
bool XapianIndex::isGood(void) const
{
	return m_goodIndex;
}

/// Gets the index location.
string XapianIndex::getLocation(void) const
{
	return m_databaseName;
}

/// Returns a document's properties.
bool XapianIndex::getDocumentInfo(unsigned int docId, DocumentInfo &docInfo) const
{
	bool foundDocument = false;

	if (docId == 0)
	{
		return false;
	}

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return false;
	}

	try
	{
		Xapian::Database *pIndex = pDatabase->readLock();
		if (pIndex != NULL)
		{
			Xapian::Document doc = pIndex->get_document(docId);

			// Get the current document data
			string record = doc.get_data();
			if (record.empty() == false)
			{
				string language = Languages::toLocale(StringManip::extractField(record, "language=", ""));

				docInfo = DocumentInfo(StringManip::extractField(record, "caption=", "\n"),
					StringManip::extractField(record, "url=", "\n"),
					StringManip::extractField(record, "type=", "\n"),
					language);
				docInfo.setTimestamp(StringManip::extractField(record, "timestamp=", "\n"));
#ifdef DEBUG
				cout << "XapianIndex::getDocumentInfo: language is "
					<< docInfo.getLanguage() << endl;
#endif
				foundDocument = true;
			}
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't get document properties: " << error.get_type() << ": " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't get document properties, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return foundDocument;
}

/// Determines whether a document has a label.
bool XapianIndex::hasLabel(unsigned int docId, const string &name) const
{
	bool foundLabel = false;

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return false;
	}

	try
	{
		Xapian::Database *pIndex = pDatabase->readLock();
		if (pIndex != NULL)
		{
			string term("XLABEL:");

			// Get documents that have this label
			// FIXME: would it be faster to get the document's terms ?
			term += name;
			Xapian::PostingIterator postingIter = pIndex->postlist_begin(term);
			if (postingIter != pIndex->postlist_end(term))
			{
				// Is this document in the list ?
				postingIter.skip_to(docId);
				if ((postingIter != pIndex->postlist_end(term)) &&
					(docId == (*postingIter)))
				{
					foundLabel = true;
				}
			}
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't check document labels: " << error.get_type() << ": " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't check document labels, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return foundLabel;
}

/// Returns a document's labels.
bool XapianIndex::getDocumentLabels(unsigned int docId, set<string> &labels) const
{
	bool gotLabels = false;

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return false;
	}

	labels.clear();
	try
	{
		Xapian::Database *pIndex = pDatabase->readLock();
		if (pIndex != NULL)
		{
			Xapian::TermIterator termIter = pIndex->termlist_begin(docId);
			if (termIter != pIndex->termlist_end(docId))
			{
				for (termIter.skip_to("XLABEL:");
					termIter != pIndex->termlist_end(docId); ++termIter)
				{
					if ((*termIter).length() < 7)
					{
						break;
					}

					// Is this a label ?
					if (strncasecmp((*termIter).c_str(), "XLABEL:", min(7, (int)(*termIter).length())) == 0)
					{
						labels.insert((*termIter).substr(7));
					}
				}
				gotLabels = true;
			}
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't get document's labels: " << error.get_type() << ": " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't get document's labels, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return gotLabels;
}

/// Checks whether the given URL is in the index.
unsigned int XapianIndex::hasDocument(const string &url) const
{
	unsigned int docId = 0;

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return 0;
	}

	try
	{
		Xapian::Database *pIndex = pDatabase->readLock();
		if (pIndex != NULL)
		{
			string term = limitTermLength(string("U") + Url::canonicalizeUrl(url), true);

			// Get documents that have this term
			Xapian::PostingIterator postingIter = pIndex->postlist_begin(term);
			if (postingIter != pIndex->postlist_end(term))
			{
				// This URL was indexed
				docId = *postingIter;
#ifdef DEBUG
				cout << "XapianIndex::hasDocument: " << term << " in document "
					<< docId << " " << postingIter.get_wdf() << " time(s)" << endl;
#endif
			}
			// FIXME: what if the term exists in more than one document ?
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't look for document: " << error.get_type() << ": " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't look for document, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return docId;
}

/// Gets terms with the same root.
unsigned int XapianIndex::getCloseTerms(const string &term, set<string> &suggestions)
{
	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return 0;
	}

	suggestions.clear();
	try
	{
		Xapian::Database *pIndex = pDatabase->readLock();
		if (pIndex != NULL)
		{
			Xapian::TermIterator termIter = pIndex->allterms_begin();

			if (termIter != pIndex->allterms_end())
			{
				string baseTerm(StringManip::toLowerCase(term));
				unsigned int count = 0;

				// Get the next 10 terms
				for (termIter.skip_to(baseTerm);
					(termIter != pIndex->allterms_end()) && (count < 10); ++termIter)
				{
					string suggestedTerm(*termIter);

					if (suggestedTerm.find(baseTerm) != 0)
					{
						// This term doesn't have the same root
						break;
					}
					suggestions.insert(*termIter);
					++count;
				}
			}
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't get terms: " << error.get_type() << ": " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't get terms, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return suggestions.size();
}

/// Returns the ID of the last document.
unsigned int XapianIndex::getLastDocumentID(void) const
{
	unsigned int docId = 0;

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return 0;
	}

	try
	{
		Xapian::Database *pIndex = pDatabase->readLock();
		if (pIndex != NULL)
		{
			docId = pIndex->get_lastdocid();
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't get last document ID: " << error.get_type() << ": " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't get last document ID, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return docId;
}

/// Returns the number of documents.
unsigned int XapianIndex::getDocumentsCount(const string &labelName) const
{
	unsigned int docCount = 0;

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return 0;
	}

	try
	{
		Xapian::Database *pIndex = pDatabase->readLock();
		if (pIndex != NULL)
		{
			if (labelName.empty() == true)
			{
				docCount = pIndex->get_doccount();
			}
			else
			{
				string term("XLABEL:");

				// Each label appears only one per document so the collection frequency
				// is the number of documents that have this label
				docCount = pIndex->get_collection_freq(term + labelName);
			}
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't count documents: " << error.get_type() << ": " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't count documents, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return docCount;
}

/// Lists document IDs.
unsigned int XapianIndex::listDocuments(set<unsigned int> &docIds,
	unsigned int maxDocsCount, unsigned int startDoc) const
{
	// Get documents that have the magic term
	return listDocumentsWithTerm(MAGIC_TERM, docIds, maxDocsCount, startDoc);
}

/// Lists documents that have a label.
bool XapianIndex::listDocumentsWithLabel(const string &name, set<unsigned int> &docIds,
	unsigned int maxDocsCount, unsigned int startDoc) const
{
	string term("XLABEL:");

	// Get documents that have this label
	term += name;
	return listDocumentsWithTerm(term, docIds, maxDocsCount, startDoc);
}
