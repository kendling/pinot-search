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

#include <algorithm>
#include "TimeConverter.h"
#include "DocumentInfo.h"

using std::string;
using std::set;
using std::copy;
using std::inserter;

DocumentInfo::DocumentInfo() :
	m_size(0)
{
	m_timestamp = TimeConverter::toTimestamp(time(NULL));
}

DocumentInfo::DocumentInfo(const string &title, const string &location,
	const string &type, const string &language) :
	m_title(title),
	m_location(location),
	m_type(type),
	m_language(language),
	m_size(0)
{
	m_timestamp = TimeConverter::toTimestamp(time(NULL));
}

DocumentInfo::DocumentInfo(const DocumentInfo &other) :
	m_title(other.m_title),
	m_location(other.m_location),
	m_type(other.m_type),
	m_language(other.m_language),
	m_timestamp(other.m_timestamp),
	m_size(other.m_size)
{
	copy(other.m_labels.begin(), other.m_labels.end(),
		inserter(m_labels, m_labels.begin()));
}

DocumentInfo::~DocumentInfo()
{
}

DocumentInfo& DocumentInfo::operator=(const DocumentInfo& other)
{
	if (this != &other)
	{
		m_title = other.m_title;
		m_location = other.m_location;
		m_type = other.m_type;
		m_language = other.m_language;
		m_timestamp = other.m_timestamp;
		m_size = other.m_size;
		m_labels.clear();
		copy(other.m_labels.begin(), other.m_labels.end(),
			inserter(m_labels, m_labels.begin()));
	}

	return *this;
}

bool DocumentInfo::operator<(const DocumentInfo& other) const
{
	if (m_location < other.m_location)
	{
		return true;
	}

	return false;
}

/// Sets the title of the document.
void DocumentInfo::setTitle(const string &title)
{
	m_title = title;
}

/// Returns the title of the document.
string DocumentInfo::getTitle(void) const
{
	return m_title;
}

/// Sets the original location of the document.
void DocumentInfo::setLocation(const string &location)
{
	m_location = location;
}

/// Returns the original location of the document.
string DocumentInfo::getLocation(void) const
{
	return m_location;
}

/// Sets the type of the document.
void DocumentInfo::setType(const string &type)
{
	m_type = type;
}

/// Returns the type of the document.
string DocumentInfo::getType(void) const
{
	return m_type;
}

/// Sets the language of the document.
void DocumentInfo::setLanguage(const string &language)
{
	m_language = language;
}

/// Returns the document's language.
string DocumentInfo::getLanguage(void) const
{
	return m_language;
}

/// Sets the document's timestamp.
void DocumentInfo::setTimestamp(const string &timestamp)
{
	m_timestamp = timestamp;
}

/// Returns the document's timestamp.
string DocumentInfo::getTimestamp(void) const
{
	return m_timestamp;
}

/// Sets the document's size in bytes.
void DocumentInfo::setSize(off_t size)
{
	m_size = size;
}

/// Returns the document's size in bytes.
off_t DocumentInfo::getSize(void) const
{
	return m_size;
}

/// Sets the document's labels.
void DocumentInfo::setLabels(const set<string> &labels)
{
	copy(labels.begin(), labels.end(),
		inserter(m_labels, m_labels.begin()));
}

/// Returns the document's labels.
const set<string> &DocumentInfo::getLabels(void) const
{
	return m_labels;
}

