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
#include "IndexInterface.h"

class XapianIndex : public IndexInterface
{
	public:
		XapianIndex(const std::string &indexName);
		virtual ~XapianIndex();

		/// Gets the index location.
		virtual std::string getLocation(void) const;

		/// Indexes the given data.
		virtual bool indexDocument(Tokenizer &tokens, const std::set<std::string> &labels,
			unsigned int &docId);

		/// Returns a document's properties.
		virtual bool getDocumentInfo(unsigned int docId, DocumentInfo &docInfo) const;

		/// Determines whether a document has a label.
		virtual bool hasLabel(unsigned int docId, const std::string &name) const;

		/// Returns a document's labels.
		virtual bool getDocumentLabels(unsigned int docId, std::set<std::string> &labels) const;

		/// Returns documents that have a label.
		virtual bool getDocumentsWithLabel(const std::string &name, std::set<unsigned int> &docIds) const;

		/// Updates the given document.
		virtual bool updateDocument(unsigned int docId, Tokenizer &tokens);

		/// Updates a document's properties.
		virtual bool updateDocumentInfo(unsigned int docId, const DocumentInfo &docInfo);

		/// Sets a document's labels.
		virtual bool setDocumentLabels(unsigned int docId, const std::set<std::string> &labels,
			bool resetLabels = true);

		/// Checks whether the given URL is in the index.
		virtual unsigned int hasDocument(const std::string &url) const;

		/// Unindexes the given document.
		virtual bool unindexDocument(unsigned int docId);

		/// Unindexes documents with the given label.
		virtual bool unindexDocuments(const std::string &labelName);

		/// Suggests terms.
		virtual unsigned int getCloseTerms(const std::string &term, std::set<std::string> &suggestions);

		/// Renames a label.
		virtual bool renameLabel(const std::string &name, const std::string &newName);

		/// Deletes all references to a label.
		virtual bool deleteLabel(const std::string &name);

		/// Flushes recent changes to the disk.
		virtual bool flush(void);

		/// Returns the number of documents.
		virtual unsigned int getDocumentsCount(void) const;

		/// Lists document IDs.
		virtual unsigned int listDocuments(std::set<unsigned int> &docIds,
			unsigned int maxDocsCount = 0, unsigned int startDoc = 0) const;

	protected:
		static const unsigned int m_maxTermLength;
		static const std::string MAGIC_TERM;
		std::string m_databaseName;
		std::string m_stemLanguage;

		void addTermsToDocument(Tokenizer &tokens, Xapian::Document &doc,
			const std::string &prefix, Xapian::termcount &termPos, StemmingMode mode) const;

		bool prepareDocument(const DocumentInfo &info, Xapian::Document &doc,
			Xapian::termcount &termPos, const std::string &summary) const;

		std::string scanDocument(const char *pData, unsigned int dataLength,
			DocumentInfo &info);

		void setDocumentData(Xapian::Document &doc, const DocumentInfo &info,
			const std::string &extract, const std::string &language) const;

	private:
		XapianIndex(const XapianIndex &other);
		XapianIndex &operator=(const XapianIndex &other);

};

#endif // _XAPIAN_INDEX_H
