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

#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <strings.h>
#include <time.h>
#include <ctype.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <utility>
#include <cstring>
#include <xapian.h>

#include "Languages.h"
#include "StringManip.h"
#include "TimeConverter.h"
#include "Url.h"
#include "LanguageDetector.h"
#include "XapianDatabaseFactory.h"
#include "XapianIndex.h"

#define MAGIC_TERM "X-MetaSE-Doc"

using std::cout;
using std::cerr;
using std::endl;
using std::ios;
using std::ifstream;
using std::ofstream;
using std::string;
using std::vector;
using std::set;
using std::map;
using std::min;
using std::max;

static bool setVersionFile(const string &databaseName, const string &version)
{
	ofstream verFile;
	string verFileName(databaseName + "/version");
	bool setVer = false;

	verFile.open(verFileName.c_str(), ios::trunc);
	if (verFile.good() == true)
	{
		verFile << version;
		setVer = true;
	}
	verFile.close();

	return setVer;
}

static string getVersionFromFile(const string &databaseName)
{
	ifstream verFile;
	string verFileName(databaseName + "/version");
	string version;

	verFile.open(verFileName.c_str());
	if (verFile.good() == true)
	{
		verFile >> version;
	}
	verFile.close();

	return version;
}

class TokensIndexer : public Dijon::CJKVTokenizer::TokensHandler
{
	public:
		TokensIndexer(Xapian::Stem *pStemmer, Xapian::Document &doc,
			const Xapian::WritableDatabase &db,
			const string &prefix, unsigned int nGramSize,
			bool &doSpelling, Xapian::termcount &termPos) :
			Dijon::CJKVTokenizer::TokensHandler(),
			m_pStemmer(pStemmer),
			m_doc(doc),
			m_db(db),
			m_prefix(prefix),
			m_nGramSize(nGramSize),
			m_nGramCount(0),
			m_doSpelling(doSpelling),
			m_termPos(termPos)
		{
		}

		virtual ~TokensIndexer()
		{
		}

		virtual bool handle_token(const string &tok, bool is_cjkv)
		{
			bool addSpelling = false;

			if (tok.empty() == true)
			{
				return false;
			}

			// Lower case the term and trim spaces
			string term(StringManip::toLowerCase(tok));
			StringManip::trimSpaces(term);

			if (term.empty() == true)
			{
				return true;
			}

			m_doc.add_posting(m_prefix + XapianDatabase::limitTermLength(term), m_termPos);

			// Is this CJKV ?
			if (is_cjkv == false)
			{
				// Don't stem if the term starts with a digit
				if ((m_pStemmer != NULL) &&
					(isdigit((int)term[0]) == 0))
				{
					string stemmedTerm((*m_pStemmer)(term));

					m_doc.add_term("Z" + XapianDatabase::limitTermLength(stemmedTerm));
				}

				addSpelling = m_doSpelling;
				++m_termPos;
				m_nGramCount = 0;
			}
			else
			{
				if (m_nGramCount % m_nGramSize == 0)
				{
					++m_termPos;
				}
				else if ((m_nGramCount + 1) % m_nGramSize == 0)
				{
					addSpelling = m_doSpelling;
				}
				++m_nGramCount;
			}

			if (addSpelling == true)
			{
				try
				{
					m_db.add_spelling(XapianDatabase::limitTermLength(term));
				}
				catch (const Xapian::UnimplementedError &error)
				{
					cerr << "Couldn't index with spelling correction: " << error.get_type() << ": " << error.get_msg() << endl;

					m_doSpelling = false;
				}
			}

			return true;
		}

	protected:
		Xapian::Stem *m_pStemmer;
		Xapian::Document &m_doc;
		const Xapian::WritableDatabase &m_db;
		string m_prefix;
		unsigned int m_nGramSize;
		unsigned int m_nGramCount;
		bool &m_doSpelling;
		Xapian::termcount &m_termPos;

};

XapianIndex::XapianIndex(const string &indexName) :
	IndexInterface(),
	m_databaseName(indexName),
	m_goodIndex(false),
	m_doSpelling(true)
{
	// Open in read-only mode
	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName);
	if ((pDatabase != NULL) &&
		(pDatabase->isOpen() == true))
	{
		m_goodIndex = true;
		m_doSpelling = pDatabase->withSpelling();
	}
}

XapianIndex::XapianIndex(const XapianIndex &other) :
	IndexInterface(other),
	m_databaseName(other.m_databaseName),
	m_goodIndex(other .m_goodIndex),
	m_doSpelling(other.m_doSpelling),
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
		m_doSpelling = other.m_doSpelling;
		m_stemLanguage = other.m_stemLanguage;
	}

	return *this;
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

void XapianIndex::addPostingsToDocument(const Xapian::Utf8Iterator &itor, Xapian::Document &doc,
	const Xapian::WritableDatabase &db, const string &prefix, bool noStemming, bool &doSpelling,
	Xapian::termcount &termPos) const
{
	Xapian::Stem *pStemmer = NULL;
	bool isCJKV = false;

	// Do we know what language to use for stemming ?
	if ((noStemming == false) &&
		(m_stemLanguage.empty() == false))
	{
		try
		{
			pStemmer = new Xapian::Stem(StringManip::toLowerCase(m_stemLanguage));
		}
		catch (const Xapian::Error &error)
		{
			cerr << "Couldn't create stemmer: " << error.get_type() << ": " << error.get_msg() << endl;
		}
	}

	const char *pRawData = itor.raw();
	if (pRawData != NULL)
	{
		Dijon::CJKVTokenizer tokenizer;
		string text(pRawData);

		if (tokenizer.has_cjkv(text) == true)
		{
			// Use overload
			addPostingsToDocument(tokenizer, pStemmer, text, doc, db,
				prefix, doSpelling, termPos);
			isCJKV = true;
		}
	}

	if (isCJKV == false)
	{
		Xapian::TermGenerator generator;

		// Set the stemmer
		if (pStemmer != NULL)
		{
			generator.set_stemmer(*pStemmer);
		}

		generator.set_termpos(termPos);
		try
		{
			// Older Xapian backends don't support spelling correction
			if (doSpelling == true)
			{
				// The database is required for the spelling dictionary
				generator.set_flags(Xapian::TermGenerator::FLAG_SPELLING);
				generator.set_database(db);
			}
			generator.set_document(doc);
			generator.index_text(itor, 1, prefix);
		}
		catch (const Xapian::UnimplementedError &error)
		{
			cerr << "Couldn't index with spelling correction: " << error.get_type() << ": " << error.get_msg() << endl;

			if (doSpelling == true)
			{
				doSpelling = false;

				// Try again without spelling correction
				// Let the caller catch the exception
				generator.set_flags(Xapian::TermGenerator::FLAG_SPELLING, Xapian::TermGenerator::FLAG_SPELLING);
				generator.set_document(doc);
				generator.index_text(itor, 1, prefix);
			}
		}
		termPos = generator.get_termpos();
	}

	if (pStemmer != NULL)
	{
		delete pStemmer;
	}
}

void XapianIndex::addPostingsToDocument(Dijon::CJKVTokenizer &tokenizer, Xapian::Stem *pStemmer,
	const string &text, Xapian::Document &doc, const Xapian::WritableDatabase &db,
	const string &prefix, bool &doSpelling, Xapian::termcount &termPos) const
{
	TokensIndexer handler(pStemmer, doc, db, prefix, tokenizer.get_ngram_size(),
		doSpelling, termPos);

	// Get the terms
	tokenizer.tokenize(text, handler);

#ifdef DEBUG
	cout << "XapianIndex::addPostingsToDocument: CJKV terms to position " << termPos << endl;
#endif

	// This will help identify which documents were processed here
	doc.add_term("XTOK:CJKV");
}

void XapianIndex::addLabelsToDocument(Xapian::Document &doc, const set<string> &labels,
	bool skipInternals)
{
	if (labels.empty() == true)
	{
		return;
	}

	for (set<string>::const_iterator labelIter = labels.begin(); labelIter != labels.end();
		++labelIter)
	{
		string labelName(*labelIter);

		// Prevent from setting internal labels ?
		if ((labelName.empty() == true) ||
			((skipInternals == true) && (labelName.substr(0, 2) == "X-")))
		{
			continue;
		}

#ifdef DEBUG
		cout << "XapianIndex::addLabelsToDocument: label \"" << labelName << "\"" << endl;
#endif
		doc.add_term(string("XLABEL:") + XapianDatabase::limitTermLength(Url::escapeUrl(labelName)));
	}
}

void XapianIndex::removePostingsFromDocument(const Xapian::Utf8Iterator &itor, Xapian::Document &doc,
	const Xapian::WritableDatabase &db, const string &prefix, const string &language,
	bool noStemming, bool &doSpelling) const
{
	Xapian::Document termsDoc;
	Xapian::termcount termPos = 0;
	bool addDoSpelling = false;

	// Get the terms, without populating the spelling database
	addPostingsToDocument(itor, termsDoc, db, prefix, noStemming, addDoSpelling, termPos);

	// Get the terms and remove the first posting for each
	for (Xapian::TermIterator termListIter = termsDoc.termlist_begin();
		termListIter != termsDoc.termlist_end(); ++termListIter)
	{
		Xapian::termcount postingsCount = termListIter.positionlist_count();
		Xapian::termcount postingNum = 0;
		bool removeTerm = false;

#ifdef DEBUG
		cout << "XapianIndex::removePostingsFromDocument: term " << *termListIter
			<< " has " << postingsCount << " postings" << endl;
#endif
		// If a prefix is defined, or there are no postings, we can afford removing the term
		if ((prefix.empty() == false) ||
			(postingsCount == 0))
		{
			removeTerm = true;
		}
		else
		{
			// Check whether this term is in the original document and how many postings it has
			Xapian::TermIterator termIter = doc.termlist_begin();
			if (termIter != doc.termlist_end())
			{
				termIter.skip_to(*termListIter);
				if (termIter != doc.termlist_end())
				{
					if (*termIter != *termListIter)
					{
						// This term doesn't exist in the document !
#ifdef DEBUG
						cout << "XapianIndex::removePostingsFromDocument: no such term" << endl;
#endif
						continue;
					}

					if (termIter.positionlist_count() <= postingsCount)
					{
						// All postings are to be removed, so we can remove the term
#ifdef DEBUG
						cout << "XapianIndex::removePostingsFromDocument: no extra posting" << endl;
#endif
						removeTerm = true;
					}
				}
			}
		}

		if (removeTerm == true)
		{
			try
			{
				doc.remove_term(*termListIter);
			}
			catch (const Xapian::Error &error)
			{
#ifdef DEBUG
				cout << "XapianIndex::removePostingsFromDocument: " << error.get_msg() << endl;
#endif
			}

			try
			{
				// Decrease this term's frequency in the spelling dictionary
				if (doSpelling == true)
				{
					db.remove_spelling(*termListIter);
				}
			}
			catch (const Xapian::UnimplementedError &error)
			{
				cerr << "Couldn't remove spelling correction: " << error.get_type() << ": " << error.get_msg() << endl;
				doSpelling = false;
			}
			catch (const Xapian::Error &error)
			{
#ifdef DEBUG
				cout << "XapianIndex::removePostingsFromDocument: " << error.get_msg() << endl;
#endif
			}
			continue;
		}

		// Otherwise, remove the first N postings
		// FIXME: if all the postings are in the range associated with the metadata
		// as opposed to the actual data, the term can be removed altogether
		for (Xapian::PositionIterator firstPosIter = termListIter.positionlist_begin();
			firstPosIter != termListIter.positionlist_end(); ++firstPosIter)
		{
			if (postingNum >= postingsCount)
			{
				break;
			}
			++postingNum;

			try
			{
				doc.remove_posting(*termListIter, *firstPosIter);
			}
			catch (const Xapian::Error &error)
			{
				// This posting may have been removed already
#ifdef DEBUG
				cout << "XapianIndex::removePostingsFromDocument: " << error.get_msg() << endl;
#endif
			}
		}
	}
}

void XapianIndex::addCommonTerms(const DocumentInfo &info, Xapian::Document &doc,
	const Xapian::WritableDatabase &db, Xapian::termcount &termPos)
{
	string title(info.getTitle());
	string location(info.getLocation());
	string type(info.getType());
	Url urlObj(location);

	// Add a magic term :-)
	doc.add_term(MAGIC_TERM);

	// Index the title with and without prefix S
	if (title.empty() == false)
	{
		addPostingsToDocument(Xapian::Utf8Iterator(title), doc, db, "S",
			true, m_doSpelling, termPos);
		addPostingsToDocument(Xapian::Utf8Iterator(title), doc, db, "",
			false, m_doSpelling, termPos);

		// Make room between common terms and what follows
		termPos += 100;
	}

	// Index the full URL with prefix U
	doc.add_term(string("U") + XapianDatabase::limitTermLength(Url::escapeUrl(location), true));
	// ...the base file with XFILE:
	string::size_type qmPos = location.find("?");
	if ((urlObj.isLocal() == true) &&
		(qmPos != string::npos))
	{
		string fileUrl(location.substr(0, qmPos));
		string protocol(urlObj.getProtocol());

		doc.add_term(string("XFILE:") + XapianDatabase::limitTermLength(Url::escapeUrl(fileUrl), true));
		if ((urlObj.isLocal() == true) &&
			(protocol != "file"))
		{
			// Add another term with file as protocol
			fileUrl.replace(0, protocol.length(), "file");
			doc.add_term(string("XFILE:") + XapianDatabase::limitTermLength(Url::escapeUrl(fileUrl), true));
		}
	}
	// ...the host name and included domains with prefix H
	string hostName(StringManip::toLowerCase(urlObj.getHost()));
	if (hostName.empty() == false)
	{
		doc.add_term(string("H") + XapianDatabase::limitTermLength(hostName, true));
		string::size_type dotPos = hostName.find('.');
		while (dotPos != string::npos)
		{
			doc.add_term(string("H") + XapianDatabase::limitTermLength(hostName.substr(dotPos + 1), true));

			// Next
			dotPos = hostName.find('.', dotPos + 1);
		}
	}
	// ...the location (as is) and all directories with prefix XDIR:
	string tree(urlObj.getLocation());
	if (tree.empty() == false)
	{
		doc.add_term(string("XDIR:") + XapianDatabase::limitTermLength(Url::escapeUrl(tree), true));
		if (tree[0] == '/')
		{
			doc.add_term("XDIR:/");
		}
		string::size_type slashPos = tree.find('/', 1);
		while (slashPos != string::npos)
		{
			doc.add_term(string("XDIR:") + XapianDatabase::limitTermLength(Url::escapeUrl(tree.substr(0, slashPos)), true));

			// Next
			slashPos = tree.find('/', slashPos + 1);
		}
	}
	// ...and the file name with prefix P
	string fileName(urlObj.getFile());
	if (fileName.empty() == false)
	{
		string extension;

		doc.add_term(string("P") + XapianDatabase::limitTermLength(Url::escapeUrl(fileName), true));

		// Does it have an extension ?
		string::size_type extPos = fileName.rfind('.');
		if ((extPos != string::npos) &&
			(extPos + 1 < fileName.length()))
		{
			extension = StringManip::toLowerCase(fileName.substr(extPos + 1));
		}
		doc.add_term(string("XEXT:") + XapianDatabase::limitTermLength(extension));
	}
	// Finally, add the language code with prefix L
	doc.add_term(string("L") + Languages::toCode(m_stemLanguage));
	// ...and the MIME type with prefix T
	doc.add_term(string("T") + type);
	string::size_type slashPos = type.find('/');
	if (slashPos != string::npos)
	{
		doc.add_term(string("XCLASS:") + type.substr(0, slashPos));
	}
}

void XapianIndex::removeCommonTerms(Xapian::Document &doc, const Xapian::WritableDatabase &db)
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

	docInfo = DocumentInfo(StringManip::extractField(record, "caption=", "\n"),
		StringManip::extractField(record, "url=", "\n"),
		StringManip::extractField(record, "type=", "\n"),
		Languages::toLocale(language));
	string modTime(StringManip::extractField(record, "modtime=", "\n"));
	if (modTime.empty() == false)
	{
		time_t timeT = (time_t )atol(modTime.c_str());
		docInfo.setTimestamp(TimeConverter::toTimestamp(timeT));
	}
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
		removePostingsFromDocument(Xapian::Utf8Iterator(title), doc, db, "S",
			language, true, m_doSpelling);
		removePostingsFromDocument(Xapian::Utf8Iterator(title), doc, db, "",
			language, false, m_doSpelling);
	}

	// Location 
	string location(docInfo.getLocation());
	commonTerms.insert(string("U") + XapianDatabase::limitTermLength(Url::escapeUrl(location), true));
	// Base file
	string::size_type qmPos = location.find("?");
	if ((urlObj.isLocal() == true) &&
		(qmPos != string::npos))
	{
		string fileUrl(location.substr(0, qmPos));
		string protocol(urlObj.getProtocol());

		commonTerms.insert(string("XFILE:") + XapianDatabase::limitTermLength(Url::escapeUrl(fileUrl), true));

		if ((urlObj.isLocal() == true) &&
			(protocol != "file"))
		{
			// Add another term with file as protocol
			fileUrl.replace(0, protocol.length(), "file");
			doc.add_term(string("XFILE:") + XapianDatabase::limitTermLength(Url::escapeUrl(fileUrl), true));
		}
	}
	// Host name
	string hostName(StringManip::toLowerCase(urlObj.getHost()));
	if (hostName.empty() == false)
	{
		commonTerms.insert(string("H") + XapianDatabase::limitTermLength(hostName, true));
		string::size_type dotPos = hostName.find('.');
		while (dotPos != string::npos)
		{
			commonTerms.insert(string("H") + XapianDatabase::limitTermLength(hostName.substr(dotPos + 1), true));

			// Next
			dotPos = hostName.find('.', dotPos + 1);
		}
	}
	// ...location
	string tree(urlObj.getLocation());
	if (tree.empty() == false)
	{
		commonTerms.insert(string("XDIR:") + XapianDatabase::limitTermLength(Url::escapeUrl(tree), true));
		if (tree[0] == '/')
		{
			commonTerms.insert("XDIR:/");
		}
		string::size_type slashPos = tree.find('/', 1);
		while (slashPos != string::npos)
		{
			commonTerms.insert(string("XDIR:") + XapianDatabase::limitTermLength(Url::escapeUrl(tree.substr(0, slashPos)), true));

			// Next
			slashPos = tree.find('/', slashPos + 1);
		}
	}
	// ...and file name
	string fileName(urlObj.getFile());
	if (fileName.empty() == false)
	{
		string extension;

		commonTerms.insert(string("P") + XapianDatabase::limitTermLength(Url::escapeUrl(fileName), true));

		// Does it have an extension ?
		string::size_type extPos = fileName.rfind('.');
		if ((extPos != string::npos) &&
			(extPos + 1 < fileName.length()))
		{
			extension = StringManip::toLowerCase(fileName.substr(extPos + 1));
		}
		commonTerms.insert(string("XEXT:") + XapianDatabase::limitTermLength(extension));
	}
	// Language code
	commonTerms.insert(string("L") + Languages::toCode(language));
	// MIME type
	string type(docInfo.getType());
	commonTerms.insert(string("T") + type);
	string::size_type slashPos = type.find('/');
	if (slashPos != string::npos)
	{
		commonTerms.insert(string("XCLASS:") + type.substr(0, slashPos));
	}

	for (set<string>::const_iterator termIter = commonTerms.begin(); termIter != commonTerms.end(); ++termIter)
	{
		try
		{
			doc.remove_term(*termIter);
		}
		catch (const Xapian::Error &error)
		{
#ifdef DEBUG
			cout << "XapianIndex::removeCommonTerms: " << error.get_msg() << endl;
#endif
		}
	}
}

string XapianIndex::scanDocument(const char *pData, unsigned int dataLength)
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
			cerr << "Couldn't create stemmer: " << error.get_type() << ": " << error.get_msg() << endl;
			continue;
		}

		language = *langIter;
		break;
	}
#ifdef DEBUG
	cout << "XapianIndex::scanDocument: language " << language << endl;
#endif

	return language;
}

void XapianIndex::setDocumentData(const DocumentInfo &info, Xapian::Document &doc,
	const string &language) const
{
	time_t timeT = TimeConverter::fromTimestamp(info.getTimestamp());
	struct tm *tm = localtime(&timeT);
	string yyyymmdd(TimeConverter::toYYYYMMDDString(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday));
	string hhmmss(TimeConverter::toHHMMSSString(tm->tm_hour, tm->tm_min, tm->tm_sec));

	// Date
	doc.add_value(0, yyyymmdd);
	// FIXME: checksum in value 1
	// Size
	doc.add_value(2, Xapian::sortable_serialise((double )info.getSize()));
	// Time
	doc.add_value(3, hhmmss);
	// Date and time, for results sorting
	doc.add_value(4, yyyymmdd + hhmmss);

	DocumentInfo docCopy(info);
	// XapianDatabase expects the language in English, which is okay here
	docCopy.setLanguage(language);
	doc.set_data(XapianDatabase::propsToRecord(&docCopy));
}

bool XapianIndex::deleteDocuments(const string &term)
{
	bool unindexed = false;

	if (term.empty() == true)
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
#ifdef DEBUG
			cout << "XapianIndex::deleteDocuments: term is " << term << endl;
#endif

			// Delete documents from the index
			pIndex->delete_document(term);

			unindexed = true;
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't unindex documents: " << error.get_type() << ": " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't unindex documents, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return unindexed;
}

//
// Implementation of IndexInterface
//

/// Returns false if the index couldn't be opened.
bool XapianIndex::isGood(void) const
{
	return m_goodIndex;
}

/// Gets metadata.
string XapianIndex::getMetadata(const string &name) const
{
#if ENABLE_XAPIAN_DB_METADATA>0
	string metadataValue;

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
			// If this index type doesn't support metadata, no exception will be thrown
			// We will just get an empty string
			metadataValue = pIndex->get_metadata(name);
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't get metadata: " << error.get_type() << ": " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't get metadata, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return metadataValue;
#else
	return "";
#endif
}

/// Sets metadata.
bool XapianIndex::setMetadata(const string &name, const string &value) const
{
#if ENABLE_XAPIAN_DB_METADATA>0
	bool setMetadata = false;

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
			pIndex->set_metadata(name, value);
			setMetadata = true;
		}
	}
	catch (const Xapian::UnimplementedError &error)
	{
		cerr << "Couldn't set metadata: " << error.get_type() << ": " << error.get_msg() << endl;
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't set metadata: " << error.get_type() << ": " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't set metadata, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return setMetadata;
#else
	return false;
#endif
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
			string record(doc.get_data());

			// Get the current document data
			if (record.empty() == false)
			{
				XapianDatabase::recordToProps(record, &docInfo);
				// XapianDatabase stored the language in English
				docInfo.setLanguage(Languages::toLocale(docInfo.getLanguage()));
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
		cerr << "Couldn't get document terms count: " << error.get_type() << ": " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't get document terms count, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return termsCount;
}

/// Returns a document's terms.
bool XapianIndex::getDocumentTerms(unsigned int docId, map<unsigned int, string> &wordsBuffer) const
{
	vector<string> noPosTerms;
	bool gotTerms = false;

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
			unsigned int lastPos = 0;

			// Go through the position list of each term
			for (Xapian::TermIterator termIter = pIndex->termlist_begin(docId);
				termIter != pIndex->termlist_end(docId); ++termIter)
			{
				string termName(*termIter);
				char firstChar = termName[0];
				bool hasPositions = false;

				// Is it prefixed ?
				if (isupper((int)firstChar) != 0)
				{
					// Skip X-prefixed terms
					if (firstChar == 'X')
					{
#ifdef DEBUG
						cout << "XapianIndex::getDocumentTerms: skipping " << termName << endl;
#endif
						continue;
					}

					// Keep other prefixed terms (S, U, H, P, L, T...)
					termName.erase(0, 1);
				}

				for (Xapian::PositionIterator positionIter = pIndex->positionlist_begin(docId, *termIter);
					positionIter != pIndex->positionlist_end(docId, *termIter); ++positionIter)
				{
					wordsBuffer[*positionIter] = termName;
					if (*positionIter > lastPos)
					{
						lastPos = *positionIter;
					}
					hasPositions = true;
				}

				if (hasPositions == false)
				{
					noPosTerms.push_back(termName);
				}

				gotTerms = true;
			}

			// Append terms without positional information as if they were at the end of the document
			for (vector<string>::const_iterator noPosIter = noPosTerms.begin();
				noPosIter != noPosTerms.end(); ++noPosIter)
			{
				wordsBuffer[lastPos] = *noPosIter;
				++lastPos;
			}
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't get document terms: " << error.get_type() << ": " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't get document terms, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return gotTerms;
}

/// Sets the list of known labels.
bool XapianIndex::setLabels(const set<string> &labels)
{
	bool setLabels = false;

#if ENABLE_XAPIAN_DB_METADATA>0
	string labelString;

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, false);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return false;
	}

	for (set<string>::const_iterator labelIter = labels.begin();
		labelIter != labels.end(); ++labelIter)
	{
		// Prevent from setting internal labels
		if (labelIter->substr(0, 2) == "X-")
		{
			continue;
		}
		labelString += "[";
		labelString += Url::escapeUrl(*labelIter);
		labelString += "]";
	}

	try
	{
#ifdef DEBUG
		cout << "XapianIndex::setLabels: " << labels.size() << " labels" << endl;
#endif
		Xapian::WritableDatabase *pIndex = pDatabase->writeLock();
		if (pIndex != NULL)
		{
			pIndex->set_metadata("labels", labelString);
			setLabels = true;
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't set database labels: " << error.get_type() << ": " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't set database labels, unknown exception occured" << endl;
	}
	pDatabase->unlock();
#endif

	return setLabels;
}

/// Gets the list of known labels.
bool XapianIndex::getLabels(set<string> &labels) const
{
#if ENABLE_XAPIAN_DB_METADATA>0
	string labelsString;

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
			labelsString = pIndex->get_metadata("labels");
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't get database labels: " << error.get_type() << ": " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't get database labels, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	if (labelsString.empty() == false)
	{
		string::size_type endPos = 0;
		string label(StringManip::extractField(labelsString, "[", "]", endPos));

		while (label.empty() == false)
		{
			labels.insert(Url::unescapeUrl(label));

			if (endPos == string::npos)
			{
				break;
			}
			label = StringManip::extractField(labelsString, "[", "]", endPos);
		}

		return true;
	}
#endif

	return false;
}

/// Adds a label.
bool XapianIndex::addLabel(const string &name)
{
	// Nothing to do here
	return false;
}

/// Renames a label.
bool XapianIndex::renameLabel(const string &name, const string &newName)
{
	bool renamedLabel = false;

	// Prevent from renaming or setting internal labels
	if ((name.substr(0, 2) == "X-") ||
		(newName.substr(0, 2) == "X-"))
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

			// Get documents that have this label
			term += XapianDatabase::limitTermLength(Url::escapeUrl(name));
			for (Xapian::PostingIterator postingIter = pIndex->postlist_begin(term);
				postingIter != pIndex->postlist_end(term); ++postingIter)
			{
				Xapian::docid docId = *postingIter;

				// Get the document
				Xapian::Document doc = pIndex->get_document(docId);
				// Remove the term
				doc.remove_term(term);
				// ...add the new one
				doc.add_term(string("XLABEL:") + XapianDatabase::limitTermLength(Url::escapeUrl(newName)));
				// ...and update the document
				pIndex->replace_document(docId, doc);
			}

			renamedLabel = true;
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't delete label: " << error.get_type() << ": " << error.get_msg() << endl;
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

	// Prevent from deleting internal labels
	if (name.substr(0, 2) == "X-")
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

			// Get documents that have this label
			term += XapianDatabase::limitTermLength(Url::escapeUrl(name));
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
		cerr << "Couldn't delete label: " << error.get_type() << ": " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't delete label, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return deletedLabel;
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
			term += XapianDatabase::limitTermLength(Url::escapeUrl(name));
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
						labels.insert(Url::unescapeUrl((*termIter).substr(7)));
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
						string term(*termIter);

						// Is this a non-internal label ?
						if ((strncasecmp(term.c_str(), "XLABEL:", min(7, (int)term.length())) == 0) &&
							(strncasecmp(term.c_str(), "XLABEL:X-", min(9, (int)term.length())) != 0))
						{
							doc.remove_term(term);
						}
					}
				}
			}

			// Set new labels
			addLabelsToDocument(doc, labels, true);

			pIndex->replace_document(docId, doc);
			updatedLabels = true;
		}
		catch (const Xapian::Error &error)
		{
			cerr << "Couldn't update document's labels: " << error.get_type() << ": " << error.get_msg() << endl;
		}
		catch (...)
		{
			cerr << "Couldn't update document's labels, unknown exception occured" << endl;
		}

		pDatabase->unlock();
	}

	return updatedLabels;
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
			string term = string("U") + XapianDatabase::limitTermLength(Url::escapeUrl(Url::canonicalizeUrl(url)), true);

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
#ifdef DEBUG
						cout << "XapianIndex::getCloseTerms: not the same root" << endl;
#endif
						break;
					}

					suggestions.insert(suggestedTerm);
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
				term += XapianDatabase::limitTermLength(Url::escapeUrl(labelName));
				docCount = pIndex->get_collection_freq(term);
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
	// All documents have the magic term
	return listDocumentsWithTerm("", docIds, maxDocsCount, startDoc);
}

/// Lists documents.
bool XapianIndex::listDocuments(const string &name, set<unsigned int> &docIds,
	NameType type, unsigned int maxDocsCount, unsigned int startDoc) const
{
	string term;

	if (type == BY_LABEL)
	{
		term = string("XLABEL:") + XapianDatabase::limitTermLength(Url::escapeUrl(name));
	}
	else if (type == BY_DIRECTORY)
	{
		term = string("XDIR:") + XapianDatabase::limitTermLength(Url::escapeUrl(name), true);
	}
	else if (type == BY_FILE)
	{
		term = string("XFILE:") + XapianDatabase::limitTermLength(Url::escapeUrl(name), true);
	}

	return listDocumentsWithTerm(term, docIds, maxDocsCount, startDoc);
}

/// Indexes the given data.
bool XapianIndex::indexDocument(const Document &document, const std::set<std::string> &labels,
	unsigned int &docId)
{
	bool indexed = false;

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, false);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return false;
	}

	// Cache the document's properties
	DocumentInfo docInfo(document.getTitle(), document.getLocation(),
		document.getType(), document.getLanguage());
	docInfo.setTimestamp(document.getTimestamp());
	docInfo.setSize(document.getSize());
	docInfo.setLocation(Url::canonicalizeUrl(docInfo.getLocation()));

	unsigned int dataLength = 0;
	const char *pData = document.getData(dataLength);

	// Don't scan the document if a language is specified
	m_stemLanguage = Languages::toEnglish(docInfo.getLanguage());
	if (m_stemLanguage.empty() == true)
	{
		if ((pData != NULL) &&
			(dataLength > 0))
		{
			m_stemLanguage = scanDocument(pData, dataLength);
			docInfo.setLanguage(Languages::toLocale(m_stemLanguage));
		}
	}

	try
	{
		Xapian::WritableDatabase *pIndex = pDatabase->writeLock();
		if (pIndex != NULL)
		{
			Xapian::Document doc;
			Xapian::termcount termPos = 0;

			// Populate the Xapian document
			addCommonTerms(docInfo, doc, *pIndex, termPos);
			if ((pData != NULL) &&
				(dataLength > 0))
			{
				Xapian::Utf8Iterator itor(pData, dataLength);
				addPostingsToDocument(itor, doc, *pIndex, "",
					false, m_doSpelling, termPos);
			}
#ifdef DEBUG
			cout << "XapianIndex::indexDocument: " << labels.size() << " labels for URL " << docInfo.getLocation() << endl;
#endif

			// Add labels
			addLabelsToDocument(doc, labels, false);

			// Set data
			setDocumentData(docInfo, doc, m_stemLanguage);

			// Add this document to the Xapian index
			docId = pIndex->add_document(doc);
			indexed = true;
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't index document: " << error.get_type() << ": " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't index document, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return indexed;
}

/// Updates the given document; true if success.
bool XapianIndex::updateDocument(unsigned int docId, const Document &document)
{
	bool updated = false;

	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, false);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return false;
	}

	// Cache the document's properties
	DocumentInfo docInfo(document.getTitle(), document.getLocation(),
		document.getType(), document.getLanguage());
	docInfo.setTimestamp(document.getTimestamp());
	docInfo.setSize(document.getSize());
	docInfo.setLocation(Url::canonicalizeUrl(docInfo.getLocation()));

	unsigned int dataLength = 0;
	const char *pData = document.getData(dataLength);

	// Don't scan the document if a language is specified
	m_stemLanguage = Languages::toEnglish(docInfo.getLanguage());
	if (m_stemLanguage.empty() == true)
	{
		if ((pData != NULL) &&
			(dataLength > 0))
		{
			m_stemLanguage = scanDocument(pData, dataLength);
			docInfo.setLanguage(Languages::toLocale(m_stemLanguage));
		}
	}

	Xapian::WritableDatabase *pIndex = NULL;

	try
	{
		set<string> labels;

		// Get the document's labels
		getDocumentLabels(docId, labels);

		pIndex = pDatabase->writeLock();
		if (pIndex != NULL)
		{
			Xapian::Document doc;
			Xapian::termcount termPos = 0;

			// Populate the Xapian document
			addCommonTerms(docInfo, doc, *pIndex, termPos);
			if ((pData != NULL) &&
				(dataLength > 0))
			{
				Xapian::Utf8Iterator itor(pData, dataLength);
				addPostingsToDocument(itor, doc, *pIndex, "",
					false, m_doSpelling, termPos);
			}

			// Add labels
			addLabelsToDocument(doc, labels, false);

			// Set data
			setDocumentData(docInfo, doc, m_stemLanguage);

			// Update the document in the database
			pIndex->replace_document(docId, doc);
			updated = true;
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't update document: " << error.get_type() << ": " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't update document, unknown exception occured" << endl;
	}
	if (pIndex != NULL)
	{
		pDatabase->unlock();
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
			Xapian::termcount termPos = 0;

			// Update the document data with the current language
			m_stemLanguage = Languages::toEnglish(docInfo.getLanguage());
			removeCommonTerms(doc, *pIndex);
			addCommonTerms(docInfo, doc, *pIndex, termPos);
			setDocumentData(docInfo, doc, m_stemLanguage);

			pIndex->replace_document(docId, doc);
			updated = true;
		}
	}
	catch (const Xapian::Error &error)
	{
		cerr << "Couldn't update document properties: " << error.get_type() << ": " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't update document properties, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return updated;
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
		cerr << "Couldn't unindex document: " << error.get_type() << ": " << error.get_msg() << endl;
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
	string term(string("U") + XapianDatabase::limitTermLength(Url::escapeUrl(Url::canonicalizeUrl(location)), true));

	return deleteDocuments(term);
}

/// Unindexes documents.
bool XapianIndex::unindexDocuments(const string &name, NameType type)
{
	string term;

	if (type == BY_LABEL)
	{
		term = string("XLABEL:") + XapianDatabase::limitTermLength(Url::escapeUrl(name));
	}
	else if (type == BY_DIRECTORY)
	{
		term = string("XDIR:") + XapianDatabase::limitTermLength(Url::escapeUrl(name), true);
	}
	else if (type == BY_FILE)
	{
		term = string("XFILE:") + XapianDatabase::limitTermLength(Url::escapeUrl(name), true);
	}

	return deleteDocuments(term);
}

/// Unindexes all documents.
bool XapianIndex::unindexAllDocuments(void)
{
	// All documents have the magic term
	return deleteDocuments(MAGIC_TERM);
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
		cerr << "Couldn't flush database: " << error.get_type() << ": " << error.get_msg() << endl;
	}
	catch (...)
	{
		cerr << "Couldn't flush database, unknown exception occured" << endl;
	}
	pDatabase->unlock();

	return flushed;
}

/// Reopens the index.
bool XapianIndex::reopen(void) const
{
	// Reopen
	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return false;
	}

	return true;
}

/// Resets the index.
bool XapianIndex::reset(void)
{
	// Overwrite and reopen
	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName, false, true);
	if (pDatabase == NULL)
	{
		cerr << "Bad index " << m_databaseName << endl;
		return false;
	}

	return true;
}

