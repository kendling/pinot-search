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

#ifndef _URL_H
#define _URL_H

#include <string>

using std::string;

class Url
{
	public:
		Url(const string &url);
		Url(const Url &other);
		virtual ~Url();

		Url& operator=(const Url& other);

		string getProtocol(void) const;
		string getUser(void) const;
		string getPassword(void) const;
		string getHost(void) const;
		string getLocation(void) const;
		string getFile(void) const;
		string getParameters(void) const;

		/// Canonicalizes an URL.
		static string canonicalizeUrl(const string &url);

		/// Truncates an URL to the given length by discarding characters in the middle.
		static string prettifyUrl(const string &url, unsigned int maxLen);

		/// Escapes an URL.
		static string escapeUrl(const string &url);

		/// Unescapes an URL.
		static string unescapeUrl(const string &escapedUrl);

	protected:
		string m_protocol;
		string m_user;
		string m_password;
		string m_host;
		string m_location;
		string m_file;
		string m_parameters;

		void parse(const string &url);

};

#endif // _URL_H
