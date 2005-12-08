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

#ifndef _MIME_SCANNER_H
#define _MIME_SCANNER_H

#include <string>

#include "Url.h"

class MIMEScanner
{
	public:
		/// Finds out the given file's MIME type.
		static std::string scanFile(const std::string &fileName);

		/// Finds out the given URL's MIME type.
		static std::string scanUrl(const Url &urlObj);

	protected:
		MIMEScanner();

		static std::string scanFileType(const std::string &fileName);

	private:
		MIMEScanner(const MIMEScanner &other);
		MIMEScanner& operator=(const MIMEScanner& other);

};

#endif // _MIME_SCANNER_H
