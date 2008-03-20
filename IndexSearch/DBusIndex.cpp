/*
 *  Copyright 2007 Fabrice Colin
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
#include <cstdlib>

#include "Languages.h"
#include "DBusIndex.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::set;
using std::map;
using std::min;

static const char *g_fieldNames[] = { "caption", "url", "type", "language", "modtime", "size", NULL };

static DBusGConnection *getBusConnection(void)
{
	GError *pError = NULL;
	DBusGConnection *pBus = NULL;

	pBus = dbus_g_bus_get(DBUS_BUS_SESSION, &pError);
	if (pBus == NULL)
	{
		if (pError != NULL)
		{
			cerr << "DBusIndex: couldn't connect to session bus: " << pError->message << endl;
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

DBusIndex::DBusIndex(IndexInterface *pROIndex) :
	IndexInterface(),
	m_pROIndex(pROIndex)
{
}

DBusIndex::DBusIndex(const DBusIndex &other) :
	IndexInterface(other),
	m_pROIndex(other.m_pROIndex)
{
}

DBusIndex::~DBusIndex()
{
	if (m_pROIndex != NULL)
	{
		// Noone else is going to delete this
		delete m_pROIndex;
	}
}

DBusIndex &DBusIndex::operator=(const DBusIndex &other)
{
	if (this != &other)
	{
		IndexInterface::operator=(other);
		m_pROIndex = other.m_pROIndex;
	}

	return *this;
}

/// Extracts docId and docInfo from a dbus message.
bool DBusIndex::documentInfoFromDBus(DBusMessageIter *iter, unsigned int &docId,
	DocumentInfo &docInfo)
{
	DBusMessageIter array_iter;
	DBusMessageIter struct_iter;

	if (iter == NULL)
	{
		return false;
	}

	int type = dbus_message_iter_get_arg_type(iter);
	if (type != DBUS_TYPE_UINT32)
	{
#ifdef DEBUG
		cout << "DBusIndex::documentInfoFromDBus: expected unsigned integer, got " << type << endl;
#endif
		return false;
	}
	dbus_message_iter_get_basic(iter, &docId);
	dbus_message_iter_next(iter);
	
	type = dbus_message_iter_get_arg_type(iter);
	if (type != DBUS_TYPE_ARRAY)
	{
#ifdef DEBUG
		cout << "DBusIndex::documentInfoFromDBus: expected array, got " << type << endl;
#endif
		return false;
	}
	dbus_message_iter_recurse(iter, &array_iter);

	do
	{
		const gchar *pName = NULL;
		const gchar *pValue = NULL;

		type = dbus_message_iter_get_arg_type(&array_iter);
		if (type != DBUS_TYPE_STRUCT)
		{
#ifdef DEBUG
			cout << "DBusIndex::documentInfoFromDBus: expected struct, got " << type << endl;
#endif
			return false;
		}

		dbus_message_iter_recurse(&array_iter, &struct_iter);
		dbus_message_iter_get_basic(&struct_iter, &pName);
		if (pName == NULL)
		{
#ifdef DEBUG
			cout << "DBusIndex::documentInfoFromDBus: invalid field name" << endl;
#endif
		}

		dbus_message_iter_next(&struct_iter);
		dbus_message_iter_get_basic(&struct_iter, &pValue);
		if (pValue == NULL)
		{
#ifdef DEBUG
			cout << "DBusIndex::documentInfoFromDBus: invalid field value" << endl;
#endif
			continue;
		}
#ifdef DEBUG
		cout << "DBusIndex::documentInfoFromDBus: field " << pName << "=" << pValue << endl;
#endif

		// Populate docInfo
		string fieldName(pName);
		if (fieldName == g_fieldNames[0])
		{
			docInfo.setTitle(pValue);
		}
		else if (fieldName == g_fieldNames[1])
		{
			docInfo.setLocation(pValue);
		}
		else if (fieldName == g_fieldNames[2])
		{
			docInfo.setType(pValue);
		}
		else if (fieldName == g_fieldNames[3])
		{
			docInfo.setLanguage(Languages::toLocale(pValue));
		}
		else if (fieldName == g_fieldNames[4])
		{
			docInfo.setTimestamp(pValue);
		}
		else if (fieldName == g_fieldNames[5])
		{
			docInfo.setSize((off_t )atoi(pValue));
		}
	}
	while (dbus_message_iter_next(&array_iter));

	return true;
}

/// Converts docId and docInfo to a dbus message.
bool DBusIndex::documentInfoToDBus(DBusMessageIter *iter, unsigned int docId,
	const DocumentInfo &docInfo)
{
        DBusMessageIter array_iter;
	DBusMessageIter struct_iter;

	if (iter == NULL)
	{
		return false;
	}

	// Append the document ID ?
	if (docId != 0)
	{
		dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT32, &docId);
	}
	if (!dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
		DBUS_STRUCT_BEGIN_CHAR_AS_STRING \
		DBUS_TYPE_STRING_AS_STRING \
		DBUS_TYPE_STRING_AS_STRING \
		DBUS_STRUCT_END_CHAR_AS_STRING, &array_iter))
	{
#ifdef DEBUG
		cout << "DBusIndex::documentInfoToDBus: couldn't open array container" << endl;
#endif
		return false;
	}

	for (unsigned int fieldNum = 0; g_fieldNames[fieldNum] != NULL; ++fieldNum)
	{
		const char *pValue = NULL;
		char sizeStr[64];

		if (!dbus_message_iter_open_container(&array_iter,
			DBUS_TYPE_STRUCT, NULL, &struct_iter))
		{
#ifdef DEBUG
			cout << "DBusIndex::documentInfoToDBus: couldn't open struct container" << endl;
#endif
			return false;
		}

		dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &g_fieldNames[fieldNum]);
		switch (fieldNum)
		{
			case 0:
				pValue = docInfo.getTitle().c_str();
				break;
			case 1:
				pValue = docInfo.getLocation().c_str();
				break;
			case 2:
				pValue = docInfo.getType().c_str();
				break;
			case 3:
				pValue = Languages::toEnglish(docInfo.getLanguage()).c_str();
				break;
			case 4:
				pValue = docInfo.getTimestamp().c_str();
				break;
			case 5:
			default:
				snprintf(sizeStr, 64, "%u", docInfo.getSize());
				pValue = sizeStr;
				break;
		}
		dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_STRING, &pValue);
#ifdef DEBUG
		cout << "DBusIndex::documentInfoToDBus: field " << g_fieldNames[fieldNum] << "=" << pValue << endl;
#endif

		if (!dbus_message_iter_close_container(&array_iter, &struct_iter))
		{
#ifdef DEBUG
			cout << "DBusIndex::documentInfoToDBus: couldn't close struct container" << endl;
#endif
			return false;
		}
	}

	if (!dbus_message_iter_close_container(iter, &array_iter))
	{
#ifdef DEBUG
		cout << "DBusIndex::documentInfoToDBus: couldn't close array container" << endl;
#endif
		return false;
	}

	return true;
}

/// Asks the D-Bus service to reload its configuration.
bool DBusIndex::reload(void)
{
	gboolean reloading = FALSE;

	DBusGConnection *pBus = getBusConnection();
	if (pBus == NULL)
	{
		return false;
	}

	DBusGProxy *pBusProxy = getBusProxy(pBus);
	if (pBusProxy == NULL)
	{
		cerr << "DBusIndex::reload: couldn't get bus proxy" << endl;
		return false;
	}

	GError *pError = NULL;
	if (dbus_g_proxy_call(pBusProxy, "Reload", &pError,
		G_TYPE_INVALID,
		G_TYPE_BOOLEAN, &reloading,
		G_TYPE_INVALID) == FALSE)
	{
		if (pError != NULL)
		{
			cerr << "DBusIndex::reload: " << pError->message << endl;
			g_error_free(pError);
		}
	}

	g_object_unref(pBusProxy);
	// FIXME: don't we have to call dbus_g_connection_unref(pBus); ?

	if (reloading == TRUE)
	{
		return true;
	}

	return false;
}


//
// Implementation of IndexInterface
//

/// Returns false if the index couldn't be opened.
bool DBusIndex::isGood(void) const
{
	if (m_pROIndex == NULL)
	{
		return false;
	}

	return m_pROIndex->isGood();
}

/// Gets metadata.
string DBusIndex::getMetadata(const string &name) const
{
	if (m_pROIndex == NULL)
	{
		return "";
	}

	return m_pROIndex->getMetadata(name);
}

/// Sets metadata.
bool DBusIndex::setMetadata(const string &name, const string &value) const
{
	cerr << "DBusIndex::setMetadata: not allowed" << endl;
	return false;
}

/// Gets the index location.
string DBusIndex::getLocation(void) const
{
	if (m_pROIndex == NULL)
	{
		return "";
	}

	return m_pROIndex->getLocation();
}

/// Returns a document's properties.
bool DBusIndex::getDocumentInfo(unsigned int docId, DocumentInfo &docInfo) const
{
	if (m_pROIndex == NULL)
	{
		return false;
	}

	reopen();

	return m_pROIndex->getDocumentInfo(docId, docInfo);
}

/// Returns a document's terms count.
unsigned int DBusIndex::getDocumentTermsCount(unsigned int docId) const
{
	if (m_pROIndex == NULL)
	{
		return false;
	}

	reopen();

	return m_pROIndex->getDocumentTermsCount(docId);
}

/// Returns a document's terms.
bool DBusIndex::getDocumentTerms(unsigned int docId,
	map<unsigned int, string> &wordsBuffer) const
{
	if (m_pROIndex == NULL)
	{
		return false;
	}

	reopen();

	return m_pROIndex->getDocumentTerms(docId, wordsBuffer);
}

/// Sets the list of known labels.
bool DBusIndex::setLabels(const set<string> &labels)
{
	// Not allowed here
	return false;
}

/// Gets the list of known labels.
bool DBusIndex::getLabels(set<string> &labels) const
{
	bool gotLabels = false;

	if (m_pROIndex != NULL)
	{
		reopen();

		return m_pROIndex->getLabels(labels);
	}

	DBusGConnection *pBus = getBusConnection();
	if (pBus == NULL)
	{
		return false;
	}

	DBusGProxy *pBusProxy = getBusProxy(pBus);
	if (pBusProxy == NULL)
	{
		cerr << "DBusIndex::getLabels: couldn't get bus proxy" << endl;
		return false;
	}

	GError *pError = NULL;
	char **pLabels;

	// G_TYPE_STRV is the GLib equivalent of DBUS_TYPE_ARRAY, DBUS_TYPE_STRING
	if (dbus_g_proxy_call(pBusProxy, "GetLabels", &pError,
		G_TYPE_INVALID,
		G_TYPE_STRV, &pLabels,
		G_TYPE_INVALID) == TRUE)
	{
		for (char **pLabel = pLabels; (*pLabel) != NULL; ++pLabel)
		{
			labels.insert(*pLabel);
		}

		// Free the array
		g_strfreev(pLabels);

		gotLabels = true;
	}
	else
	{
		if (pError != NULL)
		{
			cerr << "DBusIndex::getLabels: " << pError->message << endl;
			g_error_free(pError);
		}
	}

	g_object_unref(pBusProxy);
	// FIXME: don't we have to call dbus_g_connection_unref(pBus); ?

	return gotLabels;
}

/// Adds a label.
bool DBusIndex::addLabel(const string &name)
{
	bool addedLabel = false;

	DBusGConnection *pBus = getBusConnection();
	if (pBus == NULL)
	{
		return false;
	}

	DBusGProxy *pBusProxy = getBusProxy(pBus);
	if (pBusProxy == NULL)
	{
		cerr << "DBusIndex::addLabel: couldn't get bus proxy" << endl;
		return false;
	}

	GError *pError = NULL;
	const char *pLabel = name.c_str();

	if (dbus_g_proxy_call(pBusProxy, "AddLabel", &pError,
		G_TYPE_STRING, pLabel,
		G_TYPE_INVALID,
		G_TYPE_STRING, &pLabel,
		G_TYPE_INVALID) == TRUE)
	{
		addedLabel = true;
	}
	else
	{
		if (pError != NULL)
		{
			cerr << "DBusIndex::addLabel: " << pError->message << endl;
			g_error_free(pError);
		}
	}

	g_object_unref(pBusProxy);
	// FIXME: don't we have to call dbus_g_connection_unref(pBus); ?

	return addedLabel;
}

/// Renames a label.
bool DBusIndex::renameLabel(const string &name, const string &newName)
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
		cerr << "DBusIndex::renameLabel: couldn't get bus proxy" << endl;
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
			cerr << "DBusIndex::renameLabel: " << pError->message << endl;
			g_error_free(pError);
		}
	}

	g_object_unref(pBusProxy);
	// FIXME: don't we have to call dbus_g_connection_unref(pBus); ?

	return renamedLabel;
}

/// Deletes all references to a label.
bool DBusIndex::deleteLabel(const string &name)
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
		cerr << "DBusIndex::deleteLabel: couldn't get bus proxy" << endl;
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
			cerr << "DBusIndex::deleteLabel: " << pError->message << endl;
			g_error_free(pError);
		}
	}

	g_object_unref(pBusProxy);
	// FIXME: don't we have to call dbus_g_connection_unref(pBus); ?

	return deletedLabel;
}

/// Determines whether a document has a label.
bool DBusIndex::hasLabel(unsigned int docId, const string &name) const
{
	if (m_pROIndex == NULL)
	{
		return false;
	}

	reopen();

	return m_pROIndex->hasLabel(docId, name);
}

/// Returns a document's labels.
bool DBusIndex::getDocumentLabels(unsigned int docId, set<string> &labels) const
{
	bool gotLabels = false;

	if (m_pROIndex != NULL)
	{
		reopen();

		return m_pROIndex->getDocumentLabels(docId, labels);
	}

	DBusGConnection *pBus = getBusConnection();
	if (pBus == NULL)
	{
		return false;
	}

	DBusGProxy *pBusProxy = getBusProxy(pBus);
	if (pBusProxy == NULL)
	{
		cerr << "DBusIndex::getDocumentLabels: couldn't get bus proxy" << endl;
		return false;
	}

	GError *pError = NULL;
	char **pLabels;

	// G_TYPE_STRV is the GLib equivalent of DBUS_TYPE_ARRAY, DBUS_TYPE_STRING
	if (dbus_g_proxy_call(pBusProxy, "GetDocumentLabels", &pError,
		G_TYPE_UINT, docId,
		G_TYPE_INVALID,
		G_TYPE_STRV, &pLabels,
		G_TYPE_INVALID) == TRUE)
	{
		for (char **pLabel = pLabels; (*pLabel) != NULL; ++pLabel)
		{
			labels.insert(*pLabel);
		}

		// Free the array
		g_strfreev(pLabels);

		gotLabels = true;
	}
	else
	{
		if (pError != NULL)
		{
			cerr << "DBusIndex::getDocumentLabels: " << pError->message << endl;
			g_error_free(pError);
		}
	}

	g_object_unref(pBusProxy);
	// FIXME: don't we have to call dbus_g_connection_unref(pBus); ?

	return gotLabels;
}

/// Sets a document's labels.
bool DBusIndex::setDocumentLabels(unsigned int docId, const set<string> &labels,
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
		cerr << "DBusIndex::setDocumentLabels: couldn't get bus proxy" << endl;
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
		G_TYPE_BOOLEAN, (resetLabels == true ? TRUE : FALSE),
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
			cerr << "DBusIndex::setDocumentLabels: " << pError->message << endl;
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
bool DBusIndex::setDocumentsLabels(const set<unsigned int> &docIds,
	const set<string> &labels, bool resetLabels)
{
	gboolean updatedLabels = FALSE;

	DBusGConnection *pBus = getBusConnection();
	if (pBus == NULL)
	{
		return false;
	}

	DBusGProxy *pBusProxy = getBusProxy(pBus);
	if (pBusProxy == NULL)
	{
		cerr << "DBusIndex::setDocumentsLabels: couldn't get bus proxy" << endl;
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
		cout << "DBusIndex::setDocumentsLabels: document " << pDocIds[idIndex] << endl;
#endif
		++idIndex;
	}
	pDocIds[idIndex] = NULL;
	for (set<string>::const_iterator labelIter = labels.begin();
		labelIter != labels.end(); ++labelIter)
	{
		pLabels[labelIndex] = g_strdup(labelIter->c_str());
#ifdef DEBUG
		cout << "DBusIndex::setDocumentsLabels: label " << pLabels[labelIndex] << endl;
#endif
		++labelIndex;
	}
	pLabels[labelIndex] = NULL;

	// G_TYPE_STRV is the GLib equivalent of DBUS_TYPE_ARRAY, DBUS_TYPE_STRING
	if (dbus_g_proxy_call(pBusProxy, "SetDocumentsLabels", &pError,
		G_TYPE_STRV, pDocIds,
		G_TYPE_STRV, pLabels,
		G_TYPE_BOOLEAN, (resetLabels == true ? TRUE : FALSE),
		G_TYPE_INVALID,
		G_TYPE_BOOLEAN, &updatedLabels,
		G_TYPE_INVALID) == FALSE)
	{
		if (pError != NULL)
		{
			cerr << "DBusIndex::setDocumentsLabels: " << pError->message << endl;
			g_error_free(pError);
		}
		updatedLabels = FALSE;
	}

	// Free the arrays
	g_strfreev(pDocIds);
	g_strfreev(pLabels);

	g_object_unref(pBusProxy);
	// FIXME: don't we have to call dbus_g_connection_unref(pBus); ?

	if (updatedLabels == TRUE)
	{
		return true;
	}

	return false;
}

/// Checks whether the given URL is in the index.
unsigned int DBusIndex::hasDocument(const string &url) const
{
	if (m_pROIndex != NULL)
	{
		reopen();

		return m_pROIndex->hasDocument(url);
	}

	DBusGConnection *pBus = getBusConnection();
	if (pBus == NULL)
	{
		return false;
	}

	DBusGProxy *pBusProxy = getBusProxy(pBus);
	if (pBusProxy == NULL)
	{
		cerr << "DBusIndex::hasDocument: couldn't get bus proxy" << endl;
		return false;
	}

	GError *pError = NULL;
	const char *pUrl = url.c_str();
	unsigned int docId = 0;

	if (dbus_g_proxy_call(pBusProxy, "HasDocument", &pError,
		G_TYPE_STRING, pUrl,
		G_TYPE_INVALID,
		G_TYPE_UINT, &docId,
		G_TYPE_INVALID) == FALSE)
	{
		if (pError != NULL)
		{
			cerr << "DBusIndex::hasDocument: " << pError->message << endl;
			g_error_free(pError);
		}
	}

	g_object_unref(pBusProxy);
	// FIXME: don't we have to call dbus_g_connection_unref(pBus); ?

	return docId;
}

/// Gets terms with the same root.
unsigned int DBusIndex::getCloseTerms(const string &term, set<string> &suggestions)
{
	if (m_pROIndex == NULL)
	{
		return false;
	}

	reopen();

	return m_pROIndex->getCloseTerms(term, suggestions);
}

/// Returns the ID of the last document.
unsigned int DBusIndex::getLastDocumentID(void) const
{
	if (m_pROIndex == NULL)
	{
		return false;
	}

	reopen();

	return m_pROIndex->getLastDocumentID();
}

/// Returns the number of documents.
unsigned int DBusIndex::getDocumentsCount(const string &labelName) const
{
	if (m_pROIndex == NULL)
	{
		return false;
	}

	reopen();

	return m_pROIndex->getDocumentsCount(labelName);
}

/// Lists documents.
unsigned int DBusIndex::listDocuments(set<unsigned int> &docIds,
	unsigned int maxDocsCount, unsigned int startDoc) const
{
	if (m_pROIndex == NULL)
	{
		return false;
	}

	reopen();

	return m_pROIndex->listDocuments(docIds, maxDocsCount, startDoc);
}

/// Lists documents.
bool DBusIndex::listDocuments(const string &name, set<unsigned int> &docIds,
	NameType type, unsigned int maxDocsCount, unsigned int startDoc) const
{
	if (m_pROIndex == NULL)
	{
		return false;
	}

	reopen();

	return m_pROIndex->listDocuments(name, docIds, type, maxDocsCount, startDoc);
}

/// Indexes the given data.
bool DBusIndex::indexDocument(const Document &doc, const set<string> &labels,
	unsigned int &docId)
{
	cerr << "DBusIndex::indexDocument: not allowed" << endl;
	return false;
}

/// Updates the given document; true if success.
bool DBusIndex::updateDocument(unsigned int docId, const Document &doc)
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
		cerr << "DBusIndex::updateDocument: couldn't get bus proxy" << endl;
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
			cerr << "DBusIndex::updateDocument: " << pError->message << endl;
			g_error_free(pError);
		}
	}

	g_object_unref(pBusProxy);
	// FIXME: don't we have to call dbus_g_connection_unref(pBus); ?

	return updated;
}

/// Updates a document's properties.
bool DBusIndex::updateDocumentInfo(unsigned int docId, const DocumentInfo &docInfo)
{
	DBusMessageIter iter;
	bool updated = false;

	DBusGConnection *pBus = getBusConnection();
	if (pBus == NULL)
	{
		return false;
	}

	// FIXME: AFAIK we can't use DBusGProxy with message iterators
	DBusMessage *pMsg = dbus_message_new_method_call("de.berlios.Pinot",
		"/de/berlios/Pinot", "de.berlios.Pinot", "SetDocumentInfo");
	if (pMsg == NULL)
	{
		cerr << "DBusIndex::updateDocumentInfo: couldn't call method" << endl;
		return false;
	}

	dbus_message_iter_init_append(pMsg, &iter);
	if (DBusIndex::documentInfoToDBus(&iter, docId, docInfo) == false)
	{
		dbus_message_unref(pMsg);
	}
	else
	{
		DBusError err;

		dbus_error_init(&err);
		DBusMessage *pReply = dbus_connection_send_with_reply_and_block(dbus_g_connection_get_connection(pBus),
			pMsg, 1000 * 10, &err);
		dbus_message_unref(pMsg);

		if (dbus_error_is_set(&err))
		{
			cerr << "DBusIndex::updateDocumentInfo: " << err.message << endl;
			dbus_error_free(&err);
			return false;
		}

		if (pReply != NULL)
		{
			dbus_message_get_args(pReply, NULL,
				DBUS_TYPE_UINT32, &docId,
				DBUS_TYPE_INVALID);
			updated = true;

			dbus_message_unref(pReply);
		}
	}

	// FIXME: don't we have to call dbus_g_connection_unref(pBus); ?

	return updated;
}

/// Unindexes the given document; true if success.
bool DBusIndex::unindexDocument(unsigned int docId)
{
	cerr << "DBusIndex::unindexDocument: not allowed" << endl;
	return false;
}

/// Unindexes the given document.
bool DBusIndex::unindexDocument(const string &location)
{
	cerr << "DBusIndex::unindexDocument: not allowed" << endl;
	return false;
}

/// Unindexes documents.
bool DBusIndex::unindexDocuments(const string &name, NameType type)
{
	cerr << "DBusIndex::unindexDocuments: not allowed" << endl;
	return false;
}

/// Unindexes all documents.
bool DBusIndex::unindexAllDocuments(void)
{
	cerr << "DBusIndex::unindexDocuments: not allowed" << endl;
	return false;
}

/// Flushes recent changes to the disk.
bool DBusIndex::flush(void)
{
	// The daemon knows best when to flush
	return true;
}

/// Reopens the index.
bool DBusIndex::reopen(void) const
{
	if (m_pROIndex == NULL)
	{
		return false;
	}

	return m_pROIndex->reopen();
}

/// Resets the index.
bool DBusIndex::reset(void)
{
	// This can't be done here
	return false;
}

