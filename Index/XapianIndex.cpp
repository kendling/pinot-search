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
#include <regex.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <utility>

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

bool XapianIndex::badField(const string &field)
{
	regex_t fieldRegex;
	regmatch_t pFieldMatches[1];
	bool isBadField = false;

	// A bad field is one that includes one of our field delimiters
	if (regcomp(&fieldRegex,
		"(url|sample|caption|type|timestamp|language)=",
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
		// Does it start with a capital letter ?
		if (isupper((int)term[0]) != 0)
		{
			// R-prefix the raw term
			doc.add_posting(string("R") + term, termPos);
		}
		// Lower case the term
		term = StringManip::toLowerCase(term);

		// Stem the term ?
		if ((mode == STORE_UNSTEM) ||
			(pStemmer == NULL))
		{
			doc.add_posting(limitTermLength(prefix + term), termPos++);
		}
		else if (mode == STORE_STEM)
		{
			string stemmedTerm = pStemmer->stem_word(term);

			doc.add_posting(limitTermLength(prefix + stemmedTerm), termPos++);
		}
		else if (mode == STORE_BOTH)
		{
			string stemmedTerm = pStemmer->stem_word(term);

			// Add both
			doc.add_posting(limitTermLength(prefix + term), termPos);
			// ...at the same position
			doc.add_posting(limitTermLength(prefix + stemmedTerm), termPos++);
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
	Xapian::termcount &termPos) const
{
	string title(info.getTitle());
	string location(info.getLocation());
	Url urlObj(location);

	// Add a magic term :-)
	doc.add_term(MAGIC_TERM);

	// Index the title with and without prefix S
	if (title.empty() == false)
	{
		Document titleDoc;
		titleDoc.setData(title.c_str(), title.length());
		Tokenizer titleTokens(&titleDoc);
		addTermsToDocument(titleTokens, doc, "S", termPos, STORE_UNSTEM);
		titleTokens.rewind();
		addTermsToDocument(titleTokens, doc, "", termPos, m_stemMode);
	}

	// Index the full URL with prefix U
	doc.add_term(limitTermLength(string("U") + location, true));
	// ...the host name and included domains with prefix H
	string hostName(StringManip::toLowerCase(urlObj.getHost()));
	if (hostName.empty() == false)
	{
		doc.add_term(limitTermLength(string("H") + hostName, true));
		string::size_type dotPos = hostName.find('.');
		while (dotPos != string::npos)
		{
			doc.add_term(limitTermLength(string("H") + hostName.substr(dotPos + 1), true));

			// Next
			dotPos = hostName.find('.', dotPos + 1);
		}
	}
	// ...and the file name with prefix P
	string fileName(urlObj.getFile());
	if (fileName.empty() == false)
	{
		doc.add_term(limitTermLength(string("P") + StringManip::toLowerCase(fileName), true));
	}
	// Finally, add the language code with prefix L
	doc.add_term(string("L") + Languages::toCode(m_stemLanguage));

	setDocumentData(doc, info, m_stemLanguage);

	return true;
}

void XapianIndex::scanDocument(const char *pData, unsigned int dataLength,
	DocumentInfo &info)
{
	vector<string> candidates;
	string language;

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
			cerr << "XapianIndex::scanDocument: no support for language " << *langIter << endl;
			continue;
		}

		language = *langIter;
		break;
	}
	m_stemLanguage = language;
#ifdef DEBUG
	cout << "XapianIndex::scanDocument: language now " << m_stemLanguage << endl;
#endif

	// Update the document's properties
	string title = info.getTitle();
	if (title.empty() == true)
	{
		// Remove heading spaces
		while (isspace(title[0]))
		{
			title.erase(0, 1);
		}
		info.setTitle(title);
	}
	info.setLanguage(m_stemLanguage);
}

void XapianIndex::setDocumentData(Xapian::Document &doc, const DocumentInfo &info,
	const string &language) const
{
	string title(info.getTitle());
	string timestamp(info.getTimestamp());
	char timeStr[64];

	// Set the document data omindex-style
	string record = "url=";
	record += info.getLocation();
	// The sample will be generated at query time
	record += "\nsample=";
	record += "\ncaption=";
	if (badField(title) == true)
	{
		// Modify the title if necessary
		string::size_type pos = title.find("=");
		while (pos != string::npos)
		{
			title[pos] = ' ';
			pos = title.find("=", pos + 1);
		}
#ifdef DEBUG
		cout << "XapianIndex::setDocumentData: modified title" << endl;
#endif
	}
	record += title;
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

bool XapianIndex::listDocumentsWithTerm(const string &term, set<unsigned int> &docIds,
	unsigned int maxDocsCount, unsigned int startDoc) const
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
#ifdef DEBUG
			cout << "XapianIndex::listDocumentsWithTerm: term " << term << endl;
#endif
			// Get a list of documents that have the term
			for (Xapian::PostingIterator postingIter = pIndex->postlist_begin(term);
				(postingIter != pIndex->postlist_end(term)) && (docIds.size() < maxDocsCount);
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
		cerr << "Couldn't get document list: " << error.get_msg() << endl;
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
		docInfo.setLocation(Url::canonicalizeUrl(docInfo.getLocation()));

		scanDocument(pData, dataLength, docInfo);

		Xapian::Document doc;
		Xapian::termcount termPos = 0;

#ifdef DEBUG
		cout << "XapianIndex::indexDocument: adding terms" << endl;
#endif
		// Add the tokenizer's terms to the Xapian document
		addTermsToDocument(tokens, doc, "", termPos, m_stemMode);
		// Add labels
		for (set<string>::const_iterator labelIter = labels.begin(); labelIter != labels.end();
			++labelIter)
		{
			doc.add_term(limitTermLength(string("XLABEL:") + *labelIter));
		}
		if (prepareDocument(docInfo, doc, termPos) == true)
		{
			Xapian::WritableDatabase *pIndex = pDatabase->writeLock();
			if (pIndex != NULL)
			{
				// Add this document to the Xapian index
				docId = pIndex->add_document(doc);
				indexed = true;
			}
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
	pDatabase->unlock();

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
		cerr << "Couldn't get document properties: " << error.get_msg() << endl;
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
		cerr << "Couldn't check document labels: " << error.get_msg() << endl;
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
		cerr << "Couldn't get document's labels: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't get document's labels, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return gotLabels;
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

	const char *pData = pDocument->getData(dataLength);
	if (pData == NULL)
	{
		return false;
	}

	// Cache the document's properties
	DocumentInfo docInfo(pDocument->getTitle(), pDocument->getLocation(),
		pDocument->getType(), pDocument->getLanguage());
	docInfo.setTimestamp(pDocument->getTimestamp());
	docInfo.setLocation(Url::canonicalizeUrl(docInfo.getLocation()));

	scanDocument(pData, dataLength, docInfo);

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
				doc.add_term(limitTermLength(string("XLABEL:") + *labelIter));
			}
		}
		if (prepareDocument(docInfo, doc, termPos) == true)
		{
			Xapian::WritableDatabase *pIndex = pDatabase->writeLock();
			if (pIndex != NULL)
			{
				// Update the document in the database
				pIndex->replace_document(docId, doc);
				// FIXME: if the document information has changed, we need to update the history too
				updated = true;
			}
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
	pDatabase->unlock();

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

#ifdef DEBUG
			cout << "XapianIndex::updateDocumentInfo: language is " << docInfo.getLanguage() << endl;
#endif
			// Update the document data with the current language
			setDocumentData(doc, docInfo, docInfo.getLanguage());
			pIndex->replace_document(docId, doc);
			updated = true;
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't update document properties: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't update document properties, unknown exception occured" << endl;
	}
	pDatabase->unlock();

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
				Xapian::TermIterator termIter = pIndex->termlist_begin(docId);
				if (termIter != pIndex->termlist_end(docId))
				{
					for (termIter.skip_to("XLABEL:");
						termIter != pIndex->termlist_end(docId); ++termIter)
					{
						// Is this a label ?
						if (strncasecmp((*termIter).c_str(), "XLABEL:", min(7, (int)(*termIter).length())) == 0)
						{
							doc.remove_term(*termIter);
						}
					}
				}
			}

			// Set new labels
			for (set<string>::const_iterator labelIter = labels.begin(); labelIter != labels.end();
				++labelIter)
			{
				if (labelIter->empty() == false)
				{
					doc.add_term(limitTermLength(string("XLABEL:") + *labelIter));
				}
			}

			pIndex->replace_document(docId, doc);
			updatedLabels = true;
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't update document's labels: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't update document's labels, unknown exception occured" << endl;
	}
	pDatabase->unlock();

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
		cerr << "Couldn't look for document: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't look for document, unknown exception occured" << endl;
	}
	pDatabase->unlock();

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
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't unindex document: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't unindex document, unknown exception occured" << endl;
	}
	pDatabase->unlock();

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
			string term("XLABEL:");

			// Delete documents from the index
			term += labelName;
			pIndex->delete_document(term);
			unindexed = true;
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't unindex documents: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't unindex documents, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return unindexed;
}

/// Suggests terms.
unsigned int XapianIndex::getCloseTerms(const string &term, set<string> &suggestions)
{
	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, false);
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
				termIter.skip_to(baseTerm);
				while ((termIter != pIndex->allterms_end()) &&
					(count < 10))
				{
					string suggestedTerm(*termIter);

					if (suggestedTerm.find(baseTerm) != 0)
					{
						// This term doesn't have the same root
						break;
					}
					suggestions.insert(*termIter);

					// Next
					++count;
					++termIter;
				}
			}
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't get terms: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't get terms, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return suggestions.size();
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
			string term("XLABEL:");

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
				doc.add_term(limitTermLength(string("XLABEL:") + newName));
				// ...and update the document
				pIndex->replace_document(docId, doc);
			}

			renamedLabel = true;
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't delete label: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't delete label, unknown exception occured" << endl;
	}
	pDatabase->unlock();

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
			string term("XLABEL:");

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
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't delete label: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't delete label, unknown exception occured" << endl;
	}
	pDatabase->unlock();

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
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't flush database: " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't flush database, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return flushed;
}

/// Returns the number of documents.
unsigned int XapianIndex::getDocumentsCount(const string &labelName) const
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
		cerr << "Couldn't count documents: " << error.get_msg() << endl;
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
