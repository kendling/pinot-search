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

#ifndef _XAPIAN_INDEX_H
#define _XAPIAN_INDEX_H

#include <string>
#include <set>

#include <xapian.h>

#include "DocumentInfo.h"
#include "IndexHistory.h"
#include "IndexInterface.h"

class XapianIndex : public IndexInterface
{
	public:
		XapianIndex(const std::string &indexName);
		virtual ~XapianIndex();

		/// Gets the index location.
		virtual std::string getLocation(void) const;

		/// Indexes the given data.
		virtual bool indexDocument(Tokenizer &tokens, unsigned int &docId);

		/// Updates the given document; true if success.
		virtual bool updateDocument(unsigned int docId, Tokenizer &tokens);

		/// Returns the ID of the given document.
		virtual unsigned int hasDocument(const DocumentInfo &docInfo) const;

		/// Unindexes the given document; true if success.
		virtual bool unindexDocument(unsigned int docId);

		/// Flushes recent changes to the disk.
		virtual bool flush(void);

		/// Returns the number of documents.
		virtual unsigned int getDocumentsCount(void) const;

		/// Returns a list of document IDs.
		virtual unsigned int getDocumentIDs(std::set<unsigned int> &docIDList,
			unsigned int maxDocsCount = 0, unsigned int startDoc = 0,
			bool sortByDate = false) const;

		/// Returns a document's properties.
		virtual bool getDocumentInfo(unsigned int docId, DocumentInfo &docInfo) const;

		/// Updates a document's properties.
		virtual bool updateDocumentInfo(unsigned int docId, const DocumentInfo &docInfo);

	protected:
		static const unsigned int m_maxTermLength;
		static const std::string MAGIC_TERM;
		std::string m_databaseName;
		IndexHistory *m_pHistory;
		std::string m_stemLanguage;

		bool addTermsToDocument(Tokenizer &tokens, Xapian::Document &doc,
			Xapian::termcount &termPos, const std::string &prefix, StemmingMode mode) const;

		bool prepareDocument(const DocumentInfo &info, Xapian::Document &doc,
			Xapian::termcount &termPos, const std::string &summary) const;

		std::string scanDocument(const char *pData, unsigned int dataLength,
			DocumentInfo &info);

		void setDocumentData(Xapian::Document &doc, const DocumentInfo &info, const string &extract,
			const string &language) const;

	private:
		XapianIndex(const XapianIndex &other);
		XapianIndex &operator=(const XapianIndex &other);

};

#endif // _XAPIAN_INDEX_H
