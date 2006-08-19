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
		XapianIndex(const XapianIndex &other);
		virtual ~XapianIndex();

		XapianIndex &operator=(const XapianIndex &other);

		/// Returns false if the index couldn't be opened.
		virtual bool isGood(void) const;

		/// Gets the index location.
		virtual std::string getLocation(void) const;

		/// Returns a document's properties.
		virtual bool getDocumentInfo(unsigned int docId, DocumentInfo &docInfo) const;

		/// Determines whether a document has a label.
		virtual bool hasLabel(unsigned int docId, const std::string &name) const;

		/// Returns a document's labels.
		virtual bool getDocumentLabels(unsigned int docId, std::set<std::string> &labels) const;

		/// Checks whether the given URL is in the index.
		virtual unsigned int hasDocument(const std::string &url) const;

		/// Gets terms with the same root.
		virtual unsigned int getCloseTerms(const std::string &term, std::set<std::string> &suggestions);

		/// Returns the ID of the last document.
		virtual unsigned int getLastDocumentID(void) const;

		/// Returns the number of documents.
		virtual unsigned int getDocumentsCount(const std::string &labelName = "") const;

		/// Lists document IDs.
		virtual unsigned int listDocuments(std::set<unsigned int> &docIds,
			unsigned int maxDocsCount = 0, unsigned int startDoc = 0) const;

		/// Lists documents that have a label.
		virtual bool listDocumentsWithLabel(const std::string &name, std::set<unsigned int> &docIds,
			unsigned int maxDocsCount = 0, unsigned int startDoc = 0) const;

	protected:
		static const unsigned int m_maxTermLength;
		static const std::string MAGIC_TERM;
		std::string m_databaseName;
		bool m_goodIndex;

		static std::string limitTermLength(const std::string &term, bool makeUnique = false);

		bool listDocumentsWithTerm(const std::string &term, std::set<unsigned int> &docIds,
			unsigned int maxDocsCount = 0, unsigned int startDoc = 0) const;

};

#endif // _XAPIAN_INDEX_H
