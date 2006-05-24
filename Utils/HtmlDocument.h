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

#ifndef _HTML_DOCUMENT_H
#define _HTML_DOCUMENT_H

#include <string>

#include "Document.h"

class HtmlDocument : public Document
{
	public:
		HtmlDocument(const DocumentInfo &info);
		HtmlDocument(const HtmlDocument &other);
		virtual ~HtmlDocument();

		/// Copies the given data in the document.
		virtual bool setData(const char *data, unsigned int length);

		/// Maps the given file.
		virtual bool setDataFromFile(const std::string &fileName);

	protected:
		void parseHead(void);

};
	
#endif // _HTML_DOCUMENT_H
