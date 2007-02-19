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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <strings.h>
#include <time.h>
#include <ctype.h>
#include <regex.h>
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
using std::vector;
using std::set;
using std::min;
using std::max;

const string XapianIndex::MAGIC_TERM = "X-MetaSE-Doc";

static void removeFirstPosting(Xapian::Document &doc,
	Xapian::TermIterator &termListIter, const string &term)
{
	termListIter.skip_to(term);

	Xapian::PositionIterator firstPosIter = termListIter.positionlist_begin();
	if (firstPosIter != termListIter.positionlist_end())
	{
		try
		{
			doc.remove_posting(term, *firstPosIter);
		}
		catch (const Xapian::Error &error)
		{
			// This posting may have been removed already
#ifdef DEBUG
			cout << "XapianIndex::removeFirstPosting: " << error.get_errno() << " " << error.get_msg() << endl;
#endif
		}
	}
}

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
	m_goodIndex(other .m_goodIndex),
	m_stemLanguage(other.m_stemLanguage)
{
}

XapianIndex::~XapianIndex()
{
}

XapianIndex &XapianIndex::operator=(const XapianIndex &other)
{
	if (this != &other)
	{
		IndexInterface::operator=(other);
		m_databaseName = other.m_databaseName;
		m_goodIndex = other .m_goodIndex;
		m_stemLanguage = other.m_stemLanguage;
	}

	return *this;
}

string XapianIndex::limitTermLength(const string &term, bool makeUnique)
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
		cerr << "Couldn't get document list: " << error.get_type() << ": " << error.get_errno() << " " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't get document list, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return docIds.size();
}

void XapianIndex::addPostingsToDocument(Tokenizer &tokens, Xapian::Document &doc,
	const string &prefix, Xapian::termcount &termPos, StemmingMode mode) const
{
	Xapian::Stem *pStemmer = NULL;
	string term;

	// Do we know what language to use for stemming ?
	if (m_stemLanguage.empty() == false)
	{
		pStemmer = new Xapian::Stem(StringManip::toLowerCase(m_stemLanguage));
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
			doc.add_posting(limitTermLength(string("R") + term), termPos);
		}
		// Lower case the term
		term = StringManip::toLowerCase(term);

		// Stem the term ?
		if ((mode == STORE_UNSTEM) ||
			(pStemmer == NULL))
		{
			doc.add_posting(limitTermLength(prefix + term), termPos);
		}
		else if (mode == STORE_STEM)
		{
			string stemmedTerm = pStemmer->stem_word(term);

			doc.add_posting(limitTermLength(prefix + stemmedTerm), termPos);
		}
		else if (mode == STORE_BOTH)
		{
			string stemmedTerm = pStemmer->stem_word(term);

			// Add both at the same position
			doc.add_posting(limitTermLength(prefix + term), termPos);
			if (stemmedTerm != term)
			{
				// No point adding the same term twice
				doc.add_posting(limitTermLength(prefix + stemmedTerm), termPos);
			}
		}

		++termPos;
	}
#ifdef DEBUG
	cout << "XapianIndex::addPostingsToDocument: added " << termPos << " terms" << endl;
#endif

	if (pStemmer != NULL)
	{
		delete pStemmer;
	}
}

void XapianIndex::removeFirstPostingsFromDocument(Tokenizer &tokens, Xapian::Document &doc,
	const string &prefix, const string &language, StemmingMode mode) const
{
	Xapian::TermIterator termListIter = doc.termlist_begin();
	Xapian::Stem *pStemmer = NULL;
	string term;

	// Do we know what language to use for stemming ?
	if (language.empty() == false)
	{
		pStemmer = new Xapian::Stem(StringManip::toLowerCase(language));
	}

	// Get the terms and remove the first posting for each
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
			removeFirstPosting(doc, termListIter, string("R") + term);
		}
		// Lower case the term
		term = StringManip::toLowerCase(term);

		// Stem the term ?
		if ((mode == STORE_UNSTEM) ||
			(pStemmer == NULL))
		{
			removeFirstPosting(doc, termListIter, limitTermLength(prefix + term));
		}
		else if (mode == STORE_STEM)
		{
			removeFirstPosting(doc, termListIter, limitTermLength(prefix + pStemmer->stem_word(term)));
		}
		else if (mode == STORE_BOTH)
		{
			string stemmedTerm = pStemmer->stem_word(term);

			removeFirstPosting(doc, termListIter, limitTermLength(prefix + term));
			if (stemmedTerm != term)
			{
				removeFirstPosting(doc, termListIter, limitTermLength(prefix + stemmedTerm));
			}
		}
	}

	if (pStemmer != NULL)
	{
		delete pStemmer;
	}
}

void XapianIndex::addCommonTerms(const DocumentInfo &info, Xapian::Document &doc,
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
		addPostingsToDocument(titleTokens, doc, "S", termPos, STORE_UNSTEM);
		titleTokens.rewind();
		addPostingsToDocument(titleTokens, doc, "", termPos, m_stemMode);
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
	// ...the location (as is) and all directories with prefix XDIR:
	string tree(urlObj.getLocation());
	if (tree.empty() == false)
	{
		doc.add_term(limitTermLength(string("XDIR:") + tree, true));
		string::size_type slashPos = tree.find('/', 1);
		while (slashPos != string::npos)
		{
			doc.add_term(limitTermLength(string("XDIR:") + tree.substr(0, slashPos), true));

			// Next
			slashPos = tree.find('/', slashPos + 1);
		}
	}
	// ...and the file name with prefix P
	string fileName(urlObj.getFile());
	if (fileName.empty() == false)
	{
		string extension;

		doc.add_term(limitTermLength(string("P") + fileName, true));

		// Does it have an extension ?
		string::size_type extPos = fileName.rfind('.');
		if ((extPos != string::npos) &&
			(extPos + 1 < fileName.length()))
		{
			extension = StringManip::toLowerCase(fileName.substr(extPos + 1));
		}
		doc.add_term(limitTermLength(string("XEXT:") + extension));
	}
	// Add the date terms D, M and Y
	time_t timeT = TimeConverter::fromTimestamp(info.getTimestamp());
	struct tm *tm = localtime(&timeT);
	string yyyymmdd = TimeConverter::toYYYYMMDDString(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
	if (yyyymmdd.length() == 8)
	{
		doc.add_term(string("D") + yyyymmdd);
		doc.add_term(string("M") + yyyymmdd.substr(0, 6));
		doc.add_term(string("Y") + yyyymmdd.substr(0, 4));
	}
	// Finally, add the language code with prefix L
	doc.add_term(string("L") + Languages::toCode(m_stemLanguage));
	// ...and the MIME type with prefix T
	doc.add_term(string("T") + info.getType());
}

void XapianIndex::removeCommonTerms(Xapian::Document &doc)
{
	DocumentInfo docInfo;
	set<string> commonTerms;
	string record(doc.get_data());

	// First, remove the magic term
	commonTerms.insert(MAGIC_TERM);

	if (record.empty() == true)
        {
		// Nothing else we can do
		return;
	}

	string language(StringManip::extractField(record, "language=", "\n"));
	string timestamp(StringManip::extractField(record, "timestamp=", "\n"));

	docInfo = DocumentInfo(StringManip::extractField(record, "caption=", "\n"),
		StringManip::extractField(record, "url=", "\n"),
		StringManip::extractField(record, "type=", "\n"),
		Languages::toLocale(language));
	// We used to use timestamp prior to 0.60
	if (timestamp.empty() == true)
	{
		string modTime(StringManip::extractField(record, "modtime=", "\n"));
		if (modTime.empty() == false)
		{
			time_t timeT = (time_t )atol(modTime.c_str());
			timestamp = TimeConverter::toTimestamp(timeT);
		}
	}
	docInfo.setTimestamp(timestamp);
	string bytesSize(StringManip::extractField(record, "size=", ""));
	if (bytesSize.empty() == false)
	{
		docInfo.setSize((off_t )atol(bytesSize.c_str()));
	}
	Url urlObj(docInfo.getLocation());

	// FIXME: remove terms extracted from the title if they don't have more than one posting
	string title(docInfo.getTitle());
	if (title.empty() == false)
	{
		Document titleDoc;
		titleDoc.setData(title.c_str(), title.length());
		Tokenizer titleTokens(&titleDoc);
		removeFirstPostingsFromDocument(titleTokens, doc, "S", language, STORE_UNSTEM);
		titleTokens.rewind();
		removeFirstPostingsFromDocument(titleTokens, doc, "", language, m_stemMode);
	}

	// Title
	commonTerms.insert(limitTermLength(string("U") + docInfo.getLocation(), true));
	// Host name
	string hostName(StringManip::toLowerCase(urlObj.getHost()));
	if (hostName.empty() == false)
	{
		commonTerms.insert(limitTermLength(string("H") + hostName, true));
		string::size_type dotPos = hostName.find('.');
		while (dotPos != string::npos)
		{
			commonTerms.insert(limitTermLength(string("H") + hostName.substr(dotPos + 1), true));

			// Next
			dotPos = hostName.find('.', dotPos + 1);
		}
	}
	// ...location
	string tree(urlObj.getLocation());
	if (tree.empty() == false)
	{
		commonTerms.insert(limitTermLength(string("XDIR:") + tree, true));
		string::size_type slashPos = tree.find('/', 1);
		while (slashPos != string::npos)
		{
			commonTerms.insert(limitTermLength(string("XDIR:") + tree.substr(0, slashPos), true));

			// Next
			slashPos = tree.find('/', slashPos + 1);
		}
	}
	// ...and file name
	string fileName(urlObj.getFile());
	if (fileName.empty() == false)
	{
		string extension;

		commonTerms.insert(limitTermLength(string("P") + fileName, true));

		// Does it have an extension ?
		string::size_type extPos = fileName.rfind('.');
		if ((extPos != string::npos) &&
			(extPos + 1 < fileName.length()))
		{
			extension = StringManip::toLowerCase(fileName.substr(extPos + 1));
		}
		commonTerms.insert(limitTermLength(string("XEXT:") + extension));
	}
	// Date terms
	time_t timeT = TimeConverter::fromTimestamp(docInfo.getTimestamp());
	struct tm *tm = localtime(&timeT);
	string yyyymmdd = TimeConverter::toYYYYMMDDString(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
	if (yyyymmdd.length() == 8)
	{
		commonTerms.insert(string("D") + yyyymmdd);
		commonTerms.insert(string("M") + yyyymmdd.substr(0, 6));
		commonTerms.insert(string("Y") + yyyymmdd.substr(0, 4));
	}
	// Language code
	commonTerms.insert(string("L") + Languages::toCode(language));
	// MIME type
	commonTerms.insert(string("T") + docInfo.getType());

	for (set<string>::const_iterator termIter = commonTerms.begin(); termIter != commonTerms.end(); ++termIter)
	{
		try
		{
			doc.remove_term(*termIter);
		}
		catch (const Xapian::Error &error)
		{
#ifdef DEBUG
			cout << "XapianIndex::removeCommonTerms: " << error.get_errno() << " " << error.get_msg() << endl;
#endif
		}
	}
}

string XapianIndex::scanDocument(const char *pData, unsigned int dataLength,
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
		catch (const Xapian::Error &error)
		{
			cerr << "XapianIndex::scanDocument: " << error.get_type() << ": " << error.get_errno() << " " << error.get_msg() << endl;
			continue;
		}

		language = *langIter;
		break;
	}
#ifdef DEBUG
	cout << "XapianIndex::scanDocument: language " << language << endl;
#endif

	// Update the document's properties
	info.setLanguage(language);

	return language;
}

void XapianIndex::setDocumentData(const DocumentInfo &info, Xapian::Document &doc,
	const string &language) const
{
	string title(info.getTitle());
	string timestamp(info.getTimestamp());
	char tmpStr[64];
	time_t timeT = TimeConverter::fromTimestamp(timestamp);

	// Add this value to allow sorting by date
	doc.add_value(0, StringManip::integerToBinaryString((uint32_t)timeT));

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
	// Append a timestamp, in a format compatible with Omega
	record += "\nmodtime=";
	snprintf(tmpStr, 64, "%ld", timeT);
	record += tmpStr;
	// ...and the language
	record += "\nlanguage=";
	record += StringManip::toLowerCase(language);
	// ...and the file size
	record += "\nsize=";
	snprintf(tmpStr, 64, "%ld", info.getSize());
	record += tmpStr;
#ifdef DEBUG
	cout << "XapianIndex::setDocumentData: document data is " << record << endl;
#endif
	doc.set_data(record);
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
				string language(Languages::toLocale(StringManip::extractField(record, "language=", "\n")));
				// We used to use timestamp prior to 0.60
				string timestamp(StringManip::extractField(record, "timestamp=", "\n"));

				docInfo = DocumentInfo(StringManip::extractField(record, "caption=", "\n"),
					StringManip::extractField(record, "url=", "\n"),
					StringManip::extractField(record, "type=", "\n"),
					language);
				if (timestamp.empty() == true)
				{
					// This is the format used by Omega
					string modTime(StringManip::extractField(record, "modtime=", "\n"));
					if (modTime.empty() == false)
					{
						time_t timeT = (time_t )atol(modTime.c_str());
						timestamp = TimeConverter::toTimestamp(timeT);
					}
				}
				docInfo.setTimestamp(timestamp);
				string bytesSize(StringManip::extractField(record, "size=", ""));
				if (bytesSize.empty() == false)
				{
					docInfo.setSize((off_t )atol(bytesSize.c_str()));
				}
				foundDocument = true;
			}
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't get document properties: " << error.get_type() << ": " << error.get_errno() << " " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't get document properties, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return foundDocument;
}

/// Returns a document's terms count.
unsigned int XapianIndex::getDocumentTermsCount(unsigned int docId) const
{
	unsigned int termsCount = 0;

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
			Xapian::Document doc = pIndex->get_document(docId);

			termsCount = doc.termlist_count();
#ifdef DEBUG
			cout << "XapianIndex::getDocumentTermsCount: " << termsCount << " terms in document " << docId << endl;
#endif
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't get document terms count: " << error.get_type() << ": " << error.get_errno() << " " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't get document terms count, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return termsCount;
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
		cerr << "Couldn't check document labels: " << error.get_type() << ": " << error.get_errno() << " " << error.get_msg() << endl;
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
		cerr << "Couldn't get document's labels: " << error.get_type() << ": " << error.get_errno() << " " << error.get_msg() << endl;
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
		cerr << "Couldn't look for document: " << error.get_type() << ": " << error.get_errno() << " " << error.get_msg() << endl;
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
				bool isUpper = false;

				if (isupper((int)term[0]) != 0)
				{
					// R-prefix the term
					baseTerm = string("R") + term;
					isUpper = true;
				}

				// Get the next 10 terms
				for (termIter.skip_to(baseTerm);
					(termIter != pIndex->allterms_end()) && (count < 10); ++termIter)
				{
					string suggestedTerm(*termIter);

					if (suggestedTerm.find(baseTerm) != 0)
					{
						// This term doesn't have the same root
						if (isUpper == true)
						{
							// Try again without capital letters
							baseTerm = StringManip::toLowerCase(term);
							termIter = pIndex->allterms_begin();
							if (termIter != pIndex->allterms_end())
							{
								termIter.skip_to(baseTerm);
								isUpper = false;
								continue;
							}
						}

						break;
					}

					if (isUpper == true)
					{
						// Remove the R prefix
						suggestedTerm.erase(0, 1);
					}

					suggestions.insert(suggestedTerm);
					++count;
				}
			}
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't get terms: " << error.get_type() << ": " << error.get_errno() << " " << error.get_msg() << endl;
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
		cerr << "Couldn't get last document ID: " << error.get_type() << ": " << error.get_errno() << " " << error.get_msg() << endl;
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
		cerr << "Couldn't count documents: " << error.get_type() << ": " << error.get_errno() << " " << error.get_msg() << endl;
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

/// Lists documents that have a specific label.
bool XapianIndex::listDocumentsWithLabel(const string &name, set<unsigned int> &docIds,
	unsigned int maxDocsCount, unsigned int startDoc) const
{
	string term("XLABEL:");

	term += name;
	return listDocumentsWithTerm(term, docIds, maxDocsCount, startDoc);
}

/// Lists documents that are in a specific directory.
bool XapianIndex::listDocumentsInDirectory(const string &dirName, set<unsigned int> &docIds,
	unsigned int maxDocsCount, unsigned int startDoc) const
{
	string term("XDIR:");

	term += dirName;
	return listDocumentsWithTerm(term, docIds, maxDocsCount, startDoc);
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

		// Cache the document's properties
		DocumentInfo docInfo(pDocument->getTitle(), pDocument->getLocation(),
			pDocument->getType(), pDocument->getLanguage());
		docInfo.setTimestamp(pDocument->getTimestamp());
		docInfo.setSize(pDocument->getSize());
		docInfo.setLocation(Url::canonicalizeUrl(docInfo.getLocation()));

		const char *pData = pDocument->getData(dataLength);
		if ((pData != NULL) &&
			(dataLength > 0))
		{
			m_stemLanguage = scanDocument(pData, dataLength, docInfo);
		}

		Xapian::Document doc;
		Xapian::termcount termPos = 0;

		// Add the tokenizer's terms to the Xapian document
		addPostingsToDocument(tokens, doc, "", termPos, m_stemMode);
		// Add labels
		for (set<string>::const_iterator labelIter = labels.begin(); labelIter != labels.end();
			++labelIter)
		{
			doc.add_term(limitTermLength(string("XLABEL:") + *labelIter));
		}
		addCommonTerms(docInfo, doc, termPos);
		setDocumentData(docInfo, doc, m_stemLanguage);

		Xapian::WritableDatabase *pIndex = pDatabase->writeLock();
		if (pIndex != NULL)
		{
			// Add this document to the Xapian index
			docId = pIndex->add_document(doc);
			indexed = true;
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't index document: " << error.get_type() << ": " << error.get_errno() << " " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't index document, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return indexed;
}

/// Updates the given document; true if success.
bool XapianIndex::updateDocument(unsigned int docId, Tokenizer &tokens)
{
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

	// Cache the document's properties
	DocumentInfo docInfo(pDocument->getTitle(), pDocument->getLocation(),
		pDocument->getType(), pDocument->getLanguage());
	docInfo.setTimestamp(pDocument->getTimestamp());
	docInfo.setSize(pDocument->getSize());
	docInfo.setLocation(Url::canonicalizeUrl(docInfo.getLocation()));

	// Don't scan the document if a language is specified
	m_stemLanguage = Languages::toEnglish(pDocument->getLanguage());
	if (m_stemLanguage.empty() == true)
	{
		unsigned int dataLength = 0;
		const char *pData = pDocument->getData(dataLength);

		if ((pData != NULL) &&
			(dataLength > 0))
		{
			m_stemLanguage = scanDocument(pData, dataLength, docInfo);
		}
	}

	try
	{
		set<string> labels;
		Xapian::Document doc;
		Xapian::termcount termPos = 0;

		// Add the tokenizer's terms to the document
		addPostingsToDocument(tokens, doc, "", termPos, m_stemMode);
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
		addCommonTerms(docInfo, doc, termPos);
		setDocumentData(docInfo, doc, m_stemLanguage);

		Xapian::WritableDatabase *pIndex = pDatabase->writeLock();
		if (pIndex != NULL)
		{
			// Update the document in the database
			pIndex->replace_document(docId, doc);
			updated = true;
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't update document: " << error.get_type() << ": " << error.get_errno() << " " << error.get_msg() << endl;
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
			Xapian::termcount termPos = 0;

			// Update the document data with the current language
			removeCommonTerms(doc);
			m_stemLanguage = Languages::toEnglish(docInfo.getLanguage());
			addCommonTerms(docInfo, doc, termPos);
			setDocumentData(docInfo, doc, m_stemLanguage);

			pIndex->replace_document(docId, doc);
			updated = true;
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't update document properties: " << error.get_type() << ": " << error.get_errno() << " " << error.get_msg() << endl;
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
	set<unsigned int> docIds;

	docIds.insert(docId);
	return setDocumentsLabels(docIds, labels, resetLabels);
}

/// Sets documents' labels.
bool XapianIndex::setDocumentsLabels(const set<unsigned int> &docIds,
	const set<string> &labels, bool resetLabels)
{
	bool updatedLabels = false;

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, false);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return false;
	}

	for (set<unsigned int>::const_iterator docIter = docIds.begin();
		docIter != docIds.end(); ++docIter)
	{
		try
		{
			Xapian::WritableDatabase *pIndex = pDatabase->writeLock();
			if (pIndex == NULL)
			{
				break;
			}

			unsigned int docId = (*docIter);
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
		catch (const Xapian::Error &error)
		{
			cerr << "Couldn't update document's labels: " << error.get_type() << ": " << error.get_errno() << " " << error.get_msg() << endl;
		}
		catch (...)
		{
			cerr << "Couldn't update document's labels, unknown exception occured" << endl;
		}

		pDatabase->unlock();
	}

	return updatedLabels;
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
		cerr << "Couldn't unindex document: " << error.get_type() << ": " << error.get_errno() << " " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't unindex document, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return unindexed;
}

/// Unindexes the given document.
bool XapianIndex::unindexDocument(const string &location)
{
	bool unindexed = false;

	if (location.empty() == true)
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
			string term = limitTermLength(string("U") + Url::canonicalizeUrl(location), true);

			// Only one document should have this term
			pIndex->delete_document(term);
			unindexed = true;
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't unindex documents: " << error.get_type() << ": " << error.get_errno() << " " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't unindex documents, unknown exception occured" << endl;
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
		cerr << "Couldn't unindex documents: " << error.get_type() << ": " << error.get_errno() << " " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't unindex documents, unknown exception occured" << endl;
	}
	pDatabase->unlock();

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
		cerr << "Couldn't delete label: " << error.get_type() << ": " << error.get_errno() << " " << error.get_msg() << endl;
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
		cerr << "Couldn't delete label: " << error.get_type() << ": " << error.get_errno() << " " << error.get_msg() << endl;
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
		Xapian::WritableDatabase *pIndex = pDatabase->writeLock();
		if (pIndex != NULL)
		{
			pIndex->flush();
			flushed = true;
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't flush database: " << error.get_type() << ": " << error.get_errno() << " " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't flush database, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return flushed;
}

