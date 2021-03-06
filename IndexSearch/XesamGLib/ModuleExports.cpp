/*
 *  Copyright 2008-2012 Fabrice Colin
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

#include <string>

#include "config.h"
#include "Visibility.h"
#include "FieldMapperInterface.h"
#include "ModuleProperties.h"
#include "XesamEngine.h"

using std::string;

extern "C"
{
	PINOT_EXPORT ModuleProperties *getModuleProperties(void);
	PINOT_EXPORT SearchEngineInterface *getSearchEngine(const string &dbusObject);
	PINOT_EXPORT void setFieldMapper(FieldMapperInterface *pMapper);
	PINOT_EXPORT void closeAll(void);
}

ModuleProperties *getModuleProperties(void)
{
	return new ModuleProperties("xesam", "Xesam", "", "");
}

SearchEngineInterface *getSearchEngine(const string &dbusObject)
{
	return new XesamEngine(dbusObject);
}

void setFieldMapper(FieldMapperInterface *pMapper)
{
}

void closeAll(void)
{
}

