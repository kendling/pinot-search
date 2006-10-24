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

#ifndef _DOCUMENT_INFO_H
#define _DOCUMENT_INFO_H

#include <string>
#include <set>

class DocumentInfo
{
	public:
		DocumentInfo();
		DocumentInfo(const std::string &title, const std::string &location,
			const std::string &type, const std::string &language);
		DocumentInfo(const DocumentInfo &other);
		virtual ~DocumentInfo();

		DocumentInfo& operator=(const DocumentInfo& other);

		bool operator<(const DocumentInfo& other) const;

		/// Sets the title of the document.
		virtual void setTitle(const std::string &title);

		/// Returns the title of the document.
		virtual std::string getTitle(void) const;

		/// Sets the original location of the document.
		virtual void setLocation(const std::string &location);

		/// Returns the original location of the document.
		virtual std::string getLocation(void) const;

		/// Sets the type of the document.
		virtual void setType(const std::string &type);

		/// Returns the type of the document.
		virtual std::string getType(void) const;

		/// Sets the language of the document.
		virtual void setLanguage(const std::string &language);

		/// Returns the document's language.
		virtual std::string getLanguage(void) const;

		/// Sets the document's timestamp.
		virtual void setTimestamp(const std::string &timestamp);

		/// Returns the document's timestamp.
		virtual std::string getTimestamp(void) const;

		/// Sets the document's labels.
		virtual void setLabels(const std::set<std::string> &labels);

		/// Returns the document's labels.
		virtual const std::set<std::string> &getLabels(void) const;

	protected:
		std::string m_title;
		std::string m_location;
		std::string m_type;
		std::string m_language;
		std::string m_timestamp;
		std::set<std::string> m_labels;

};

#endif // _DOCUMENT_INFO_H
