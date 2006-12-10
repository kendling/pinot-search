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

#ifndef _DBUS_XAPIAN_INDEX_H
#define _DBUS_XAPIAN_INDEX_H

#include <string>
#include <set>

#include "XapianIndex.h"

/// Allows to write to the daemon index via D-Bus. 
class DBusXapianIndex : public XapianIndex
{
	public:
		DBusXapianIndex(const std::string &indexName);
		DBusXapianIndex(const DBusXapianIndex &other);
		virtual ~DBusXapianIndex();

		DBusXapianIndex &operator=(const DBusXapianIndex &other);

		/// Asks the D-Bus service for statistics.
		static bool getStatistics(unsigned int &crawledCount, unsigned int &docsCount);

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

		/// Lists documents that have a specific label.
		virtual bool listDocumentsWithLabel(const std::string &name, std::set<unsigned int> &docIds,
			unsigned int maxDocsCount = 0, unsigned int startDoc = 0) const;

		/// Lists documents that are in a specific directory.
		virtual bool listDocumentsInDirectory(const std::string &dirName, std::set<unsigned int> &docIds,
			unsigned int maxDocsCount = 0, unsigned int startDoc = 0) const;

		/// Indexes the given data.
		virtual bool indexDocument(Tokenizer &tokens, const std::set<std::string> &labels,
			unsigned int &docId);

		/// Updates the given document.
		virtual bool updateDocument(unsigned int docId, Tokenizer &tokens);

		/// Updates a document's properties.
		virtual bool updateDocumentInfo(unsigned int docId, const DocumentInfo &docInfo);

		/// Sets a document's labels.
		virtual bool setDocumentLabels(unsigned int docId, const std::set<std::string> &labels,
			bool resetLabels = true);

		/// Unindexes the given document.
		virtual bool unindexDocument(unsigned int docId);

		/// Unindexes documents with the given label.
		virtual bool unindexDocuments(const std::string &labelName);

		/// Renames a label.
		virtual bool renameLabel(const std::string &name, const std::string &newName);

		/// Deletes all references to a label.
		virtual bool deleteLabel(const std::string &name);

		/// Flushes recent changes to the disk.
		virtual bool flush(void);

	protected:
		void reopen(void) const;

};

#endif // _DBUS_XAPIAN_INDEX_H
