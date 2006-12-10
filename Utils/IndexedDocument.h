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

#ifndef _INDEXED_DOCUMENT_H
#define _INDEXED_DOCUMENT_H

#include <string>

#include "Document.h"

/// An entry in the index.
class IndexedDocument : public Document
{
	public:
		IndexedDocument(const std::string &title, const std::string &location,
			const std::string &originalLocation, const std::string &type,
			const std::string &language);
		IndexedDocument(const IndexedDocument &other);
		virtual ~IndexedDocument();

		IndexedDocument& operator=(const IndexedDocument& other);

		/// Returns the document ID.
		virtual unsigned int getID(void) const;

		/// Sets the original location of the document.
		virtual void setOriginalLocation(const std::string &originalLocation);

		/// Returns the original location of the document.
		virtual std::string getOriginalLocation(void) const;

	protected:
		std::string m_originalLocation;

};
	
#endif // _INDEXED_DOCUMENT_H
