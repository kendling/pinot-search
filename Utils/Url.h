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

#ifndef _URL_H
#define _URL_H

#include <string>

class Url
{
	public:
		Url(const std::string &url);
		Url(const Url &other);
		virtual ~Url();

		Url& operator=(const Url& other);

		/// Canonicalizes an URL.
		static std::string canonicalizeUrl(const std::string &url);

		/// Truncates an URL to the given length by discarding characters in the middle.
		static std::string prettifyUrl(const std::string &url, unsigned int maxLen);

		/// Reduces a host name to the given TLD level.
		static std::string reduceHost(const std::string &hostName, unsigned int level);

		/// Escapes an URL.
		static std::string escapeUrl(const std::string &url);

		/// Unescapes an URL.
		static std::string unescapeUrl(const std::string &escapedUrl);

		std::string getProtocol(void) const;
		std::string getUser(void) const;
		std::string getPassword(void) const;
		std::string getHost(void) const;
		std::string getLocation(void) const;
		std::string getFile(void) const;
		std::string getParameters(void) const;
		bool isLocal(void) const ;

	protected:
		std::string m_protocol;
		std::string m_user;
		std::string m_password;
		std::string m_host;
		std::string m_location;
		std::string m_file;
		std::string m_parameters;

		void parse(const std::string &url);

		bool isLocal(const std::string &protocol) const;

};

#endif // _URL_H
