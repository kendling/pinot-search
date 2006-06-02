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

#ifndef _WEB_ENGINE_H
#define _WEB_ENGINE_H

#include <string>

#include "SearchEngineInterface.h"
#include "Document.h"

class WebEngine : public SearchEngineInterface
{
	public:
		WebEngine();
		virtual ~WebEngine();

	protected:
		std::string m_hostFilter;
		std::string m_fileFilter;

		Document *downloadPage(const DocumentInfo &docInfo);

	private:
		WebEngine(const WebEngine &other);
		WebEngine &operator=(const WebEngine &other);

};

#endif // _WEB_ENGINE_H
