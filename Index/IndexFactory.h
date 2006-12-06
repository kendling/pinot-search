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

#ifndef _INDEX_FACTORY_H
#define _INDEX_FACTORY_H

#include <string>

#include "IndexInterface.h"

class IndexFactory
{
	public:
		virtual ~IndexFactory();

		/// Returns an index of the specified type; NULL if unavailable.
		static IndexInterface *getIndex(const std::string &type, const std::string &option);

	protected:
		IndexFactory();

	private:
		IndexFactory(const IndexFactory &other);
		IndexFactory &operator=(const IndexFactory &other);

};

#endif // _INDEX_FACTORY_H
