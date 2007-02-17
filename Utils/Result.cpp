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

#include "Result.h"

using std::string;

Result::Result(const string &location, const string &title, const string &extract,
	const string &language, float percentageScore) :
	DocumentInfo(title, location, "", language),
	m_extract(extract),
	m_score(percentageScore)
{
}

Result::Result(const Result &other) :
	DocumentInfo(other),
	m_extract(other.m_extract),
	m_score(other.m_score),
	m_indexId(other.m_indexId),
	m_docId(other.m_docId)
{
}

Result::~Result()
{
}

Result &Result::operator=(const Result &other)
{
	if (this != &other)
	{
		DocumentInfo::operator=(other);
		m_extract = other.m_extract;
		m_score = other.m_score;
		m_indexId = other.m_indexId;
		m_docId = other.m_docId;
	}

	return *this;
}

bool Result::operator<(const Result& other) const
{
	if (DocumentInfo::operator<(other) == false)
	{
		if (m_score < other.m_score)
		{
			return true;
		}

		return false;
	}

	return true;
}

/// Sets the result extract.
void Result::setExtract(const string &extract)
{
	m_extract = extract;
}

/// Gets the result extract.
string Result::getExtract(void) const
{
	return m_extract;
}

/// Gets the result score.
float Result::getScore(void) const
{
	return m_score;
}

/// Sets that the result is indexed.
void Result::setIsIndexed(unsigned int indexId, unsigned int docId)
{
	m_indexId = indexId;
	m_docId = docId;
}

/// Sets that the result is not indexed.
void Result::setIsNotIndexed(void)
{
	m_indexId = 0;
	m_docId = 0;
}

/// Gets whether the result is indexed.
bool Result::getIsIndexed(void) const
{
	if (m_docId > 0)
	{
		return true;
	}

	return false;
}

/// Gets whether the result is indexed.
unsigned int Result::getIsIndexed(unsigned int &indexId) const
{
	indexId = m_indexId;

	return m_docId;
}
