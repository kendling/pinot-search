/*
 *  Copyright 2012 Fabrice Colin
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
#ifndef _FIELD_MAPPER_INTERFACE_H
#define _FIELD_MAPPER_INTERFACE_H

#include <string>
#include <map>

#include "DocumentInfo.h"
#include "Visibility.h"

/// Interface implemented by field mappers.
class PINOT_EXPORT FieldMapperInterface
{
	public:
		FieldMapperInterface(const FieldMapperInterface &other) {};
		virtual ~FieldMapperInterface() {};

		/// Maps to terms and their prefixes.
		virtual void mapTerms(const DocumentInfo &docInfo,
			std::map<std::string, std::string> &prefixedTerms) = 0;

		/// Saves terms as record data.
		virtual void toRecord(const DocumentInfo *pDocInfo,
			std::string &record) = 0;

		/// Retrieves terms from record data.
		virtual void fromRecord(DocumentInfo *pDocInfo,
			const std::string &record) = 0;

	protected:
		FieldMapperInterface() { };

};

#endif // _FIELD_MAPPER_INTERFACE_H
