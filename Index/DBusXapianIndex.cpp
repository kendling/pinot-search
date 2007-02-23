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

#include <iostream>
extern "C"
{
#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
}

#include "Languages.h"
#include "XapianDatabaseFactory.h"
#include "DBusXapianIndex.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::set;
using std::min;

static DBusGConnection *getBusConnection(void)
{
	GError *pError = NULL;
	DBusGConnection *pBus = NULL;

	pBus = dbus_g_bus_get(DBUS_BUS_SESSION, &pError);
	if (pBus == NULL)
	{
		if (pError != NULL)
		{
			cerr << "DBusXapianIndex: couldn't connect to session bus: " << pError->message << endl;
			g_error_free(pError);
		}
	}

	return pBus;
}

static DBusGProxy *getBusProxy(DBusGConnection *pBus)
{
	if (pBus == NULL)
	{
		return NULL;
	}

	return dbus_g_proxy_new_for_name(pBus,
		"de.berlios.Pinot", "/de/berlios/Pinot", "de.berlios.Pinot");
}

DBusXapianIndex::DBusXapianIndex(const string &indexName) :
	XapianIndex(indexName)
{
}

DBusXapianIndex::DBusXapianIndex(const DBusXapianIndex &other) :
	XapianIndex(other)
{
}

DBusXapianIndex::~DBusXapianIndex()
{
}

DBusXapianIndex &DBusXapianIndex::operator=(const DBusXapianIndex &other)
{
	if (this != &other)
	{
		XapianIndex::operator=(other);
	}

	return *this;
}

void DBusXapianIndex::reopen(void) const
{
	XapianDatabase *pDatabase = XapianDatabaseFactory::getDatabase(m_databaseName);
	if (pDatabase != NULL)
	{
		// Re-open the database to the latest available version
		pDatabase->reopen();
#ifdef DEBUG
		cout << "DBusXapianIndex::reopen: done" << endl;
#endif
	}
}

/// Asks the D-Bus service for statistics.
bool DBusXapianIndex::getStatistics(unsigned int &crawledCount, unsigned int &docsCount)
{
	bool gotStats = false;

	DBusGConnection *pBus = getBusConnection();
	if (pBus == NULL)
	{
		return false;
	}

	DBusGProxy *pBusProxy = getBusProxy(pBus);
	if (pBusProxy == NULL)
	{
		cerr << "DBusXapianIndex::getStatistics: couldn't get bus proxy" << endl;
		return false;
	}

	GError *pError = NULL;
	if (dbus_g_proxy_call(pBusProxy, "GetStatistics", &pError,
		G_TYPE_INVALID,
		G_TYPE_UINT, &crawledCount,
		G_TYPE_UINT, &docsCount,
		G_TYPE_INVALID) == TRUE)
	{
		gotStats = true;
	}
	else
	{
		if (pError != NULL)
		{
			cerr << "DBusXapianIndex::getStatistics: " << pError->message << endl;
			g_error_free(pError);
		}
	}

	g_object_unref(pBusProxy);
	// FIXME: don't we have to call dbus_g_connection_unref(pBus); ?

	return gotStats;
}


//
// Implementation of IndexInterface
//

/// Returns false if the index couldn't be opened.
bool DBusXapianIndex::isGood(void) const
{
	return XapianIndex::isGood();
}

/// Gets the index location.
string DBusXapianIndex::getLocation(void) const
{
	return XapianIndex::getLocation();
}

/// Returns a document's properties.
bool DBusXapianIndex::getDocumentInfo(unsigned int docId, DocumentInfo &docInfo) const
{
	reopen();

	return XapianIndex::getDocumentInfo(docId, docInfo);
}

/// Returns a document's terms count.
unsigned int DBusXapianIndex::getDocumentTermsCount(unsigned int docId) const
{
	return XapianIndex::getDocumentTermsCount(docId);
}

/// Determines whether a document has a label.
bool DBusXapianIndex::hasLabel(unsigned int docId, const string &name) const
{
	reopen();

	return XapianIndex::hasLabel(docId, name);
}

/// Returns a document's labels.
bool DBusXapianIndex::getDocumentLabels(unsigned int docId, set<string> &labels) const
{
	reopen();

	return XapianIndex::getDocumentLabels(docId, labels);
}

/// Checks whether the given URL is in the index.
unsigned int DBusXapianIndex::hasDocument(const string &url) const
{
	reopen();

	return XapianIndex::hasDocument(url);
}

/// Gets terms with the same root.
unsigned int DBusXapianIndex::getCloseTerms(const string &term, set<string> &suggestions)
{
	reopen();

	return XapianIndex::getCloseTerms(term, suggestions);
}

/// Returns the ID of the last document.
unsigned int DBusXapianIndex::getLastDocumentID(void) const
{
	reopen();

	return XapianIndex::getLastDocumentID();
}

/// Returns the number of documents.
unsigned int DBusXapianIndex::getDocumentsCount(const string &labelName) const
{
	reopen();

	return XapianIndex::getDocumentsCount(labelName);
}

/// Lists document IDs.
unsigned int DBusXapianIndex::listDocuments(set<unsigned int> &docIds,
	unsigned int maxDocsCount, unsigned int startDoc) const
{
	reopen();

	return XapianIndex::listDocuments(docIds, maxDocsCount, startDoc);
}

/// Lists documents that have a specific label.
bool DBusXapianIndex::listDocumentsWithLabel(const string &name, set<unsigned int> &docIds,
	unsigned int maxDocsCount, unsigned int startDoc) const
{
	reopen();

	return XapianIndex::listDocumentsWithLabel(name, docIds, maxDocsCount, startDoc);
}

/// Lists documents that have a specific directory.
bool DBusXapianIndex::listDocumentsInDirectory(const string &dirName, set<unsigned int> &docIds,
	unsigned int maxDocsCount, unsigned int startDoc) const
{
	reopen();

	return XapianIndex::listDocumentsWithLabel(dirName, docIds, maxDocsCount, startDoc);
}

/// Indexes the given data.
bool DBusXapianIndex::indexDocument(Tokenizer &tokens, const set<string> &labels,
	unsigned int &docId)
{
	cerr << "DBusXapianIndex::indexDocument: not allowed" << endl;
	return false;
}

/// Updates the given document; true if success.
bool DBusXapianIndex::updateDocument(unsigned int docId, Tokenizer &tokens)
{
	bool updated = false;

	DBusGConnection *pBus = getBusConnection();
	if (pBus == NULL)
	{
		return false;
	}

	DBusGProxy *pBusProxy = getBusProxy(pBus);
	if (pBusProxy == NULL)
	{
		cerr << "DBusXapianIndex::updateDocument: couldn't get bus proxy" << endl;
		return false;
	}

	GError *pError = NULL;
	if (dbus_g_proxy_call(pBusProxy, "UpdateDocument", &pError,
		G_TYPE_UINT, docId,
		G_TYPE_INVALID,
		G_TYPE_UINT, &docId,
		G_TYPE_INVALID) == TRUE)
	{
		updated = true;
	}
	else
	{
		if (pError != NULL)
		{
			cerr << "DBusXapianIndex::updateDocument: " << pError->message << endl;
			g_error_free(pError);
		}
	}

	g_object_unref(pBusProxy);
	// FIXME: don't we have to call dbus_g_connection_unref(pBus); ?

	return updated;
}

/// Updates a document's properties.
bool DBusXapianIndex::updateDocumentInfo(unsigned int docId, const DocumentInfo &docInfo)
{
	bool updated = false;

	DBusGConnection *pBus = getBusConnection();
	if (pBus == NULL)
	{
		return false;
	}

	DBusGProxy *pBusProxy = getBusProxy(pBus);
	if (pBusProxy == NULL)
	{
		cerr << "DBusXapianIndex::updateDocumentInfo: couldn't get bus proxy" << endl;
		return false;
	}

	GError *pError = NULL;
	const char *pTitle = docInfo.getTitle().c_str();
	const char *pLocation = docInfo.getLocation().c_str();
	const char *pType = docInfo.getType().c_str();
	string language(Languages::toEnglish(docInfo.getLanguage()));
	const char *pLanguage = language.c_str();

	if (dbus_g_proxy_call(pBusProxy, "SetDocumentInfo", &pError,
		G_TYPE_UINT, docId,
		G_TYPE_STRING, pTitle,
		G_TYPE_STRING, pLocation,
		G_TYPE_STRING, pType,
		G_TYPE_STRING, pLanguage,
		G_TYPE_INVALID,
		G_TYPE_UINT, &docId,
		G_TYPE_INVALID) == TRUE)
	{
		updated = true;
	}
	else
	{
		if (pError != NULL)
		{
			cerr << "DBusXapianIndex::updateDocumentInfo: " << pError->message << endl;
			g_error_free(pError);
		}
	}

	g_object_unref(pBusProxy);
	// FIXME: don't we have to call dbus_g_connection_unref(pBus); ?

	return updated;
}

/// Sets a document's labels.
bool DBusXapianIndex::setDocumentLabels(unsigned int docId, const set<string> &labels,
	bool resetLabels)
{
	bool updatedLabels = false;

	DBusGConnection *pBus = getBusConnection();
	if (pBus == NULL)
	{
		return false;
	}

	DBusGProxy *pBusProxy = getBusProxy(pBus);
	if (pBusProxy == NULL)
	{
		cerr << "DBusXapianIndex::setDocumentLabels: couldn't get bus proxy" << endl;
		return false;
	}

	GError *pError = NULL;
	dbus_uint32_t labelsCount = labels.size();
	char **pLabels;
	unsigned int labelIndex = 0;

	pLabels = g_new(char *, labelsCount + 1);
	for (set<string>::const_iterator labelIter = labels.begin();
		labelIter != labels.end(); ++labelIter)
	{
		pLabels[labelIndex] = g_strdup(labelIter->c_str());
		++labelIndex;
	}
	pLabels[labelIndex] = NULL;

	// G_TYPE_STRV is the GLib equivalent of DBUS_TYPE_ARRAY, DBUS_TYPE_STRING
	if (dbus_g_proxy_call(pBusProxy, "SetDocumentLabels", &pError,
		G_TYPE_UINT, docId,
		G_TYPE_STRV, pLabels,
		G_TYPE_BOOLEAN, resetLabels,
		G_TYPE_INVALID,
		G_TYPE_UINT, &docId,
		G_TYPE_INVALID) == TRUE)
	{
		updatedLabels = true;
	}
	else
	{
		if (pError != NULL)
		{
			cerr << "DBusXapianIndex::setDocumentLabels: " << pError->message << endl;
			g_error_free(pError);
		}
	}

	// Free the array
	g_strfreev(pLabels);

	g_object_unref(pBusProxy);
	// FIXME: don't we have to call dbus_g_connection_unref(pBus); ?

	return updatedLabels;
}

/// Sets documents' labels.
bool DBusXapianIndex::setDocumentsLabels(const set<unsigned int> &docIds,
	const set<string> &labels, bool resetLabels)
{
	bool updatedLabels = false;

	DBusGConnection *pBus = getBusConnection();
	if (pBus == NULL)
	{
		return false;
	}

	DBusGProxy *pBusProxy = getBusProxy(pBus);
	if (pBusProxy == NULL)
	{
		cerr << "DBusXapianIndex::setDocumentsLabels: couldn't get bus proxy" << endl;
		return false;
	}

	GError *pError = NULL;
	dbus_uint32_t idsCount = docIds.size();
	dbus_uint32_t labelsCount = labels.size();
	char **pDocIds;
	char **pLabels;
	unsigned int idIndex = 0, labelIndex = 0;

	pDocIds = g_new(char *, idsCount + 1);
	pLabels = g_new(char *, labelsCount + 1);
	for (set<unsigned int>::const_iterator idIter = docIds.begin();
		idIter != docIds.end(); ++idIter)
	{
		pDocIds[idIndex] = g_strdup_printf("%u", *idIter); 
#ifdef DEBUG
		cout << "DBusXapianIndex::setDocumentsLabels: document " << pDocIds[idIndex] << endl;
#endif
		++idIndex;
	}
	pDocIds[idIndex] = NULL;
	for (set<string>::const_iterator labelIter = labels.begin();
		labelIter != labels.end(); ++labelIter)
	{
		pLabels[labelIndex] = g_strdup(labelIter->c_str());
#ifdef DEBUG
		cout << "DBusXapianIndex::setDocumentsLabels: label " << pLabels[labelIndex] << endl;
#endif
		++labelIndex;
	}
	pLabels[labelIndex] = NULL;

	// G_TYPE_STRV is the GLib equivalent of DBUS_TYPE_ARRAY, DBUS_TYPE_STRING
	if (dbus_g_proxy_call(pBusProxy, "SetDocumentsLabels", &pError,
		G_TYPE_STRV, pDocIds,
		G_TYPE_STRV, pLabels,
		G_TYPE_BOOLEAN, resetLabels,
		G_TYPE_INVALID,
		G_TYPE_BOOLEAN, &updatedLabels,
		G_TYPE_INVALID) == FALSE)
	{
		if (pError != NULL)
		{
			cerr << "DBusXapianIndex::setDocumentsLabels: " << pError->message << endl;
			g_error_free(pError);
		}
		updatedLabels = false;
	}

	// Free the arrays
	g_strfreev(pDocIds);
	g_strfreev(pLabels);

	g_object_unref(pBusProxy);
	// FIXME: don't we have to call dbus_g_connection_unref(pBus); ?

	return updatedLabels;
}

/// Unindexes the given document; true if success.
bool DBusXapianIndex::unindexDocument(unsigned int docId)
{
	cerr << "DBusXapianIndex::unindexDocument: not allowed" << endl;
	return false;
}

/// Unindexes documents with the given label or under the given directory.
bool DBusXapianIndex::unindexDocuments(const string &name, bool isDirectory)
{
	cerr << "DBusXapianIndex::unindexDocuments: not allowed" << endl;
	return false;
}

/// Renames a label.
bool DBusXapianIndex::renameLabel(const string &name, const string &newName)
{
	bool renamedLabel = false;

	DBusGConnection *pBus = getBusConnection();
	if (pBus == NULL)
	{
		return false;
	}

	DBusGProxy *pBusProxy = getBusProxy(pBus);
	if (pBusProxy == NULL)
	{
		cerr << "DBusXapianIndex::renameLabel: couldn't get bus proxy" << endl;
		return false;
	}

	GError *pError = NULL;
	const char *pOldLabel = name.c_str();
	const char *pNewLabel = newName.c_str();

	if (dbus_g_proxy_call(pBusProxy, "RenameLabel", &pError,
		G_TYPE_STRING, pOldLabel,
		G_TYPE_STRING, pNewLabel,
		G_TYPE_INVALID,
		G_TYPE_STRING, &pNewLabel,
		G_TYPE_INVALID) == TRUE)
	{
		renamedLabel = true;
	}
	else
	{
		if (pError != NULL)
		{
			cerr << "DBusXapianIndex::renameLabel: " << pError->message << endl;
			g_error_free(pError);
		}
	}

	g_object_unref(pBusProxy);
	// FIXME: don't we have to call dbus_g_connection_unref(pBus); ?

	return renamedLabel;
}

/// Deletes all references to a label.
bool DBusXapianIndex::deleteLabel(const string &name)
{
	bool deletedLabel = false;

	DBusGConnection *pBus = getBusConnection();
	if (pBus == NULL)
	{
		return false;
	}

	DBusGProxy *pBusProxy = getBusProxy(pBus);
	if (pBusProxy == NULL)
	{
		cerr << "DBusXapianIndex::deleteLabel: couldn't get bus proxy" << endl;
		return false;
	}

	GError *pError = NULL;
	const char *pLabel = name.c_str();

	if (dbus_g_proxy_call(pBusProxy, "DeleteLabel", &pError,
		G_TYPE_STRING, pLabel,
		G_TYPE_INVALID,
		G_TYPE_STRING, &pLabel,
		G_TYPE_INVALID) == TRUE)
	{
		deletedLabel = true;
	}
	else
	{
		if (pError != NULL)
		{
			cerr << "DBusXapianIndex::deleteLabel: " << pError->message << endl;
			g_error_free(pError);
		}
	}

	g_object_unref(pBusProxy);
	// FIXME: don't we have to call dbus_g_connection_unref(pBus); ?

	return deletedLabel;
}

/// Flushes recent changes to the disk.
bool DBusXapianIndex::flush(void)
{
	// There is no method for this because the daemon knows best when to flush
	return true;
}
