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
#include <time.h>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "StringManip.h"
#include "Url.h"
#include "Summarizer.h"
#include "TimeConverter.h"
#include "LanguageDetector.h"
#include "XapianDatabaseFactory.h"
#include "XapianIndex.h"

using std::string;
using std::set;

// This puts a limit to terms length.
const unsigned int XapianIndex::m_maxTermLength = 64;
const string XapianIndex::MAGIC_TERM = "X-MetaSE-Doc";

XapianIndex::XapianIndex(const string &indexName) :
	IndexInterface(),
	m_databaseName(indexName)
{

	string historyFile = indexName;
	historyFile += "/history";

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, false);
	if ((pDatabase != NULL) &&
		(pDatabase->isOpen() == true))
	{
		m_goodIndex = true;
	}
}

XapianIndex::~XapianIndex()
{
}

void XapianIndex::addTermsToDocument(Tokenizer &tokens, Xapian::Document &doc,
	const string &prefix, Xapian::termcount &termPos, StemmingMode mode) const
{
	Xapian::Stem *pStemmer = NULL;
	string term;

	// Do we know what language to use for stemming ?
	if (m_stemLanguage.empty() == false)
	{
		pStemmer = new Xapian::Stem(m_stemLanguage);
	}

	// Get the terms
	while (tokens.nextToken(term) == true)
	{
		if (term.empty() == true)
		{
			continue;
		}
		// Lower case the term
		term = StringManip::toLowerCase(term);

		// Stem the term ?
		if ((mode == STORE_UNSTEM) ||
			(pStemmer == NULL))
		{
			doc.add_posting(prefix + term,  termPos++);
		}
		else if (mode == STORE_STEM)
		{
			string stemmedTerm = pStemmer->stem_word(term);

			doc.add_posting(prefix + stemmedTerm,  termPos++);
		}
		else if (mode == STORE_BOTH)
		{
			string stemmedTerm = pStemmer->stem_word(term);

			// Add both
			doc.add_posting(prefix + term,  termPos);
			// ...at the same position
			doc.add_posting(prefix + stemmedTerm,  termPos++);
		}
	}
#ifdef DEBUG
	cout << "XapianIndex::addTermsToDocument: added " << termPos << " terms" << endl;
#endif

	if (pStemmer != NULL)
	{
		delete pStemmer;
	}
}

bool XapianIndex::prepareDocument(const DocumentInfo &info, Xapian::Document &doc,
	Xapian::termcount &termPos, const std::string &summary) const
{
	// Add a magic term :-)
	doc.add_term(MAGIC_TERM);

	// Index the title with and without prefix T
	string title = info.getTitle();
	if (title.empty() == false)
	{
		Document titleDoc;
		titleDoc.setData(title.c_str(), title.length());
		Tokenizer titleTokens(&titleDoc);
		addTermsToDocument(titleTokens, doc, "T", termPos, STORE_UNSTEM);
		titleTokens.rewind();
		addTermsToDocument(titleTokens, doc, "", termPos, m_stemMode);
	}

	string location = info.getLocation();
	Url urlObj(location);

	// Index the full URL with prefix U
	doc.add_term(string("U") + location);
	// ...the host name with prefix H
	string hostName = urlObj.getHost();
	doc.add_term(string("H") + StringManip::toLowerCase(hostName));
	// ...and the file name with prefix F
	string fileName = urlObj.getFile();
	doc.add_term(string("F") + StringManip::toLowerCase(fileName));
	// Finally, add the language with prefix L
	doc.add_term(string("L") + StringManip::toLowerCase(m_stemLanguage));

	setDocumentData(doc, info, summary, m_stemLanguage);

	return true;
}

string XapianIndex::scanDocument(const char *pData, unsigned int dataLength,
	DocumentInfo &info)
{
	vector<string> candidates;
	string language;
	string summary;

	// Try to determine the document's language
	LanguageDetector lang;
	lang.guessLanguage(pData, max(dataLength, (unsigned int)2048), candidates);

	// See which of these languages is suitable for stemming
	for (vector<string>::iterator langIter = candidates.begin(); langIter != candidates.end(); ++langIter)
	{
		if (*langIter == "unknown")
		{
			continue;
		}

		try
		{
			Xapian::Stem stemmer(*langIter);
		}
		catch (const Xapian::Error &e)
		{
#ifdef DEBUG
			cout << "XapianIndex::scanDocument: no support for " << *langIter << endl;
#endif
			continue;
		}

		language = *langIter;
		break;
	}
	m_stemLanguage = language;
#ifdef DEBUG
	cout << "XapianIndex::scanDocument: language now " << m_stemLanguage << endl;
#endif

	// Get a summary of the document
	if (language.empty() == true)
	{
		// Fall back on English
		language = "english";
	}
	Summarizer sum(language, 100);
	summary = sum.summarize(pData, dataLength);

	// Update the document's properties
	string title = info.getTitle();
	if (title.empty() == true)
	{
		// Use the title supplied by the summarizer
		title = sum.getTitle();
		// Remove heading spaces
		while (isspace(title[0]))
		{
			title.erase(0, 1);
		}
		info.setTitle(title);
	}
	info.setLanguage(m_stemLanguage);

	return summary;
}

void XapianIndex::setDocumentData(Xapian::Document &doc, const DocumentInfo &info, const string &extract,
	const string &language) const
{
	char timeStr[64];
	string timestamp = info.getTimestamp();

	// Set the document data omindex-style
	string record = "url=";
	record += info.getLocation();
	record += "\nsample=";
	record += extract;
	record += "\ncaption=";
	record += info.getTitle();
	record += "\ntype=";
	record += info.getType();
	// Append a timestamp
	record += "\ntimestamp=";
	record += timestamp;
	// ...and the language
	record += "\nlanguage=";
	record += language;
#ifdef DEBUG
	cout << "XapianIndex::setDocumentData: document data is " << record << endl;
#endif
	doc.set_data(record);

	// Add this value to allow sorting by date
	snprintf(timeStr, 64, "%d", TimeConverter::fromTimestamp(timestamp));
	doc.add_value(0, timeStr);
}

//
// Implementation of IndexInterface
//

/// Gets the index location.
string XapianIndex::getLocation(void) const
{
	return m_databaseName;
}

/// Indexes the given data.
bool XapianIndex::indexDocument(Tokenizer &tokens, const std::set<std::string> &labels,
			unsigned int &docId)
{
	unsigned int dataLength = 0;
	bool indexed = false;

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, false);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return false;
	}

	try
	{
		// Get the document
		const Document *pDocument = tokens.getDocument();
		if (pDocument == NULL)
		{
#ifdef DEBUG
			cout << "XapianIndex::indexDocument: no document" << endl;
#endif
			return false;
		}

		// Obtain a summary
		const char *pData = pDocument->getData(dataLength);
		if (pData == NULL)
		{
#ifdef DEBUG
			cout << "XapianIndex::indexDocument: empty document" << endl;
#endif
			return false;
		}
		// Cache the document's properties
		DocumentInfo docInfo(pDocument->getTitle(), pDocument->getLocation(),
			pDocument->getType(), pDocument->getLanguage());
		docInfo.setTimestamp(pDocument->getTimestamp());

		string summary = scanDocument(pData, dataLength, docInfo);

#ifdef DEBUG
		cout << "XapianIndex::indexDocument: adding terms" << endl;
#endif
		Xapian::Document doc;
		Xapian::termcount termPos = 0;

		// Add the tokenizer's terms to the Xapian document
		addTermsToDocument(tokens, doc, "", termPos, m_stemMode);
		// Add labels
		for (set<string>::const_iterator labelIter = labels.begin(); labelIter != labels.end();
			++labelIter)
		{
			doc.add_term(string("C") + *labelIter);
		}
		if (prepareDocument(docInfo, doc, termPos, summary) == true)
		{
			Xapian::WritableDatabase *pIndex = pDatabase->writeLock();
			if (pIndex != NULL)
			{
				// Add this document to the Xapian index
				docId = pIndex->add_document(doc);
				indexed = true;
			}
			pDatabase->unlock();
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't index document: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't index document, unknown exception occured" << endl;
	}

	return indexed;
}

/// Returns a document's properties.
bool XapianIndex::getDocumentInfo(unsigned int docId, DocumentInfo &docInfo) const
{
	bool foundDocument = false;

	if (docId == 0)
	{
		return false;
	}

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, false);
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
				docInfo = DocumentInfo(StringManip::extractField(record, "caption=", "\n"),
					StringManip::extractField(record, "url=", "\n"),
					StringManip::extractField(record, "type=", "\n"),
					StringManip::extractField(record, "language=", "\n"));
				docInfo.setTimestamp(StringManip::extractField(record, "timestamp=", "\n"));
				foundDocument = true;
			}
		}
		pDatabase->unlock();
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't get document properties: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't get document properties, unknown exception occured" << endl;
	}

	return foundDocument;
}

/// Returns a document's labels.
bool XapianIndex::getDocumentLabels(unsigned int docId, set<string> &labels) const
{
	bool gotLabels = false;

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, false);
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
			for (Xapian::TermIterator termIter = pIndex->termlist_begin(docId);
				termIter != pIndex->termlist_end(docId); ++termIter)
			{
				// Is this a label ?
				if ((*termIter)[0] == 'C')
				{
					labels.insert((*termIter).substr(1));
				}
			}
			gotLabels = true;
		}
		pDatabase->unlock();
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't get document's labels: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't get document's labels, unknown exception occured" << endl;
	}

	return gotLabels;
}

/// Returns documents that have a label.
bool XapianIndex::getDocumentsWithLabel(const string &name, set<unsigned int> &docIds) const
{
	bool lookedForLabel = false;

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, false);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return false;
	}

	docIds.clear();
	try
	{
		Xapian::WritableDatabase *pIndex = pDatabase->writeLock();
		if (pIndex != NULL)
		{
			string term("C");

			// Get documents that have this label
			term += name;
			for (Xapian::PostingIterator postingIter = pIndex->postlist_begin(term);
				postingIter != pIndex->postlist_end(term); ++postingIter)
			{
				Xapian::docid docId = *postingIter;

				docIds.insert(docId);
			}
			lookedForLabel = true;
		}
		pDatabase->unlock();
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't get documents: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't get documents, unknown exception occured" << endl;
	}

	return lookedForLabel;
}

/// Updates the given document; true if success.
bool XapianIndex::updateDocument(unsigned int docId, Tokenizer &tokens)
{
	unsigned int dataLength = 0;
	bool updated = false;

	const Document *pDocument = tokens.getDocument();
	if (pDocument == NULL)
	{
		return false;
	}

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, false);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return false;
	}

	// Obtain a summary
	const char *pData = pDocument->getData(dataLength);
	if (pData == NULL)
	{
		return false;
	}

	// Cache the document's properties
	DocumentInfo docInfo(pDocument->getTitle(), pDocument->getLocation(),
		pDocument->getType(), pDocument->getLanguage());
	docInfo.setTimestamp(pDocument->getTimestamp());

	string summary = scanDocument(pData, dataLength, docInfo);

	try
	{
		set<string> labels;
		Xapian::Document doc;
		Xapian::termcount termPos = 0;

		// Add the tokenizer's terms to the document
		 addTermsToDocument(tokens, doc, "", termPos, m_stemMode);
		// Get the document's labels
		if (getDocumentLabels(docId, labels) == true)
		{
			// Add labels
			for (set<string>::const_iterator labelIter = labels.begin(); labelIter != labels.end();
				++labelIter)
			{
				doc.add_term(string("C") + *labelIter);
			}
		}
		if (prepareDocument(docInfo, doc, termPos, summary) == true)
		{
			Xapian::WritableDatabase *pIndex = pDatabase->writeLock();
			if (pIndex != NULL)
			{
				// Update the document in the database
				pIndex->replace_document(docId, doc);
				// FIXME: if the document information has changed, we need to update the history too
				updated = true;
			}
			pDatabase->unlock();
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't update document: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't update document, unknown exception occured" << endl;
	}

	return updated;
}

/// Updates a document's properties.
bool XapianIndex::updateDocumentInfo(unsigned int docId, const DocumentInfo &docInfo)
{
	bool updated = false;

	if (docId == 0)
	{
		return false;
	}

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, false);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return false;
	}

	try
	{
		Xapian::WritableDatabase *pIndex = pDatabase->writeLock();
		if (pIndex != NULL)
		{
			Xapian::Document doc = pIndex->get_document(docId);

			// Get the current document data
			string record = doc.get_data();
			string extract = StringManip::extractField(record, "sample=", "\n");
			string language = StringManip::extractField(record, "language=", "\n");

			// Update the document data with the current extract and language
			setDocumentData(doc, docInfo, extract, language);
			pIndex->replace_document(docId, doc);
			updated = true;
		}
		pDatabase->unlock();
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't update document properties: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't update document properties, unknown exception occured" << endl;
	}

	return updated;
}

/// Sets a document's labels.
bool XapianIndex::setDocumentLabels(unsigned int docId, const set<string> &labels,
	bool resetLabels)
{
	bool updatedLabels = false;

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, false);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return false;
	}

	try
	{
		Xapian::WritableDatabase *pIndex = pDatabase->writeLock();
		if (pIndex != NULL)
		{
			Xapian::Document doc = pIndex->get_document(docId);

			// Reset existing labels ?
			if (resetLabels == true)
			{
				for (Xapian::TermIterator termIter = doc.termlist_begin();
					termIter != doc.termlist_end(); ++termIter)
				{
					// Is this a label ?
					if ((*termIter)[0] == 'C')
					{
						doc.remove_term(*termIter);
					}
				}
			}

			// Set new labels
			for (set<string>::const_iterator labelIter = labels.begin(); labelIter != labels.end();
				++labelIter)
			{
				doc.add_term(string("C") + *labelIter);
			}

			pIndex->replace_document(docId, doc);
			updatedLabels = true;
		}
		pDatabase->unlock();
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't update document's labels: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't update document's labels, unknown exception occured" << endl;
	}

	return updatedLabels;
}

/// Checks whether the given URL is in the index.
unsigned int XapianIndex::hasDocument(const string &url) const
{
	unsigned int docId = 0;

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, false);
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
			string term("U");

			// Get documents that have this term
			term += url;
			Xapian::PostingIterator postingIter = pIndex->postlist_begin(term);
			if (postingIter != pIndex->postlist_end(term))
			{
				// This URL was indexed
				docId = *postingIter;
			}
			// FIXME: what if the term exist in more than one document ?
		}
		pDatabase->unlock();
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't delete label: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't delete label, unknown exception occured" << endl;
	}

	return docId;
}

/// Unindexes the given document; true if success.
bool XapianIndex::unindexDocument(unsigned int docId)
{
	bool unindexed = false;

	if (docId == 0)
	{
		return false;
	}

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, false);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return false;
	}

	try
	{
		Xapian::WritableDatabase *pIndex = pDatabase->writeLock();
		if (pIndex != NULL)
		{
			// Delete the document from the index
			pIndex->delete_document(docId);
			unindexed = true;
		}
		pDatabase->unlock();
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't unindex document: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't unindex document, unknown exception occured" << endl;
	}

	return unindexed;
}

/// Unindexes documents with the given label.
bool XapianIndex::unindexDocuments(const string &labelName)
{
	bool unindexed = false;

	if (labelName.empty() == true)
	{
		return false;
	}

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, false);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return false;
	}

	try
	{
		Xapian::WritableDatabase *pIndex = pDatabase->writeLock();
		if (pIndex != NULL)
		{
			string term("C");

			// Delete documents from the index
			term += labelName;
			pIndex->delete_document(term);
			unindexed = true;
		}
		pDatabase->unlock();
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't unindex documents: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't unindex documents, unknown exception occured" << endl;
	}

	return unindexed;
}

/// Renames a label.
bool XapianIndex::renameLabel(const string &name, const string &newName)
{
	bool renamedLabel = false;

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, false);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return false;
	}

	try
	{
		Xapian::WritableDatabase *pIndex = pDatabase->writeLock();
		if (pIndex != NULL)
		{
			string term("C");

			// Get documents that have this label
			term += name;
			for (Xapian::PostingIterator postingIter = pIndex->postlist_begin(term);
				postingIter != pIndex->postlist_end(term); ++postingIter)
			{
				Xapian::docid docId = *postingIter;

				// Get the document
				Xapian::Document doc = pIndex->get_document(docId);
				// Remove the term
				doc.remove_term(term);
				// ...add the new one
				doc.add_term(string("C") + newName);
				// ...and update the document
				pIndex->replace_document(docId, doc);
			}

			renamedLabel = true;
		}
		pDatabase->unlock();
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't delete label: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't delete label, unknown exception occured" << endl;
	}

	return renamedLabel;
}

/// Deletes all references to a label.
bool XapianIndex::deleteLabel(const string &name)
{
	bool deletedLabel = false;

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, false);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return false;
	}

	try
	{
		Xapian::WritableDatabase *pIndex = pDatabase->writeLock();
		if (pIndex != NULL)
		{
			string term("C");

			// Get documents that have this label
			term += name;
			for (Xapian::PostingIterator postingIter = pIndex->postlist_begin(term);
				postingIter != pIndex->postlist_end(term); ++postingIter)
			{
				Xapian::docid docId = *postingIter;

				// Get the document
				Xapian::Document doc = pIndex->get_document(docId);
				// Remove the term
				doc.remove_term(term);
				// ...and update the document
				pIndex->replace_document(docId, doc);
			}
			deletedLabel = true;
		}
		pDatabase->unlock();
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't delete label: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't delete label, unknown exception occured" << endl;
	}

	return deletedLabel;
}

/// Flushes recent changes to the disk.
bool XapianIndex::flush(void)
{
	bool flushed = false;

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, false);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return false;
	}

	try
	{
#ifdef DEBUG
		cout << "XapianIndex::flush: called" << endl;
#endif
		Xapian::WritableDatabase *pIndex = pDatabase->writeLock();
		if (pIndex != NULL)
		{
			pIndex->flush();
			flushed = true;
		}
		pDatabase->unlock();
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't flush database: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't flush database, unknown exception occured" << endl;
	}

	return flushed;
}

/// Returns the number of documents.
unsigned int XapianIndex::getDocumentsCount(void) const
{
	unsigned int docCount = 0;

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, false);
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
			docCount = pIndex->get_doccount();
		}
		pDatabase->unlock();
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't count documents: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't count documents, unknown exception occured" << endl;
	}

	return docCount;
}

/// Returns a list of document IDs.
unsigned int XapianIndex::getDocumentIDs(set<unsigned int> &docIds,
	unsigned int maxDocsCount, unsigned int startDoc, bool sortByDate) const
{
	unsigned int docCount = 0;

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, false);
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
			// Get a list of documents that have the magic term
			for (Xapian::PostingIterator postingIter = pIndex->postlist_begin(MAGIC_TERM);
				(postingIter != pIndex->postlist_end(MAGIC_TERM)) && (docIds.size() < maxDocsCount);
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
		pDatabase->unlock();
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't get document list: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't get document list, unknown exception occured" << endl;
	}

	return docIds.size();
}
