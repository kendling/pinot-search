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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>

#include "Document.h"
#include "TimeConverter.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;

Document::Document() :
	DocumentInfo(),
	m_pData(NULL),
	m_dataLength(0),
	m_isMapped(false)
{
}

Document::Document(const string &title, const string &location,
	const string &type, const string &language) :
	DocumentInfo(title, location, type, language),
	m_pData(NULL),
	m_dataLength(0),
	m_isMapped(false)
{
}

Document::Document(const DocumentInfo &info) :
	DocumentInfo(info),
	m_pData(NULL),
	m_dataLength(0),
	m_isMapped(false)
{
}

Document::Document(const Document &other) :
	DocumentInfo(other),
	m_pData(NULL),
	m_dataLength(0),
	m_isMapped(false)
{
	// Copying does a deep copy
	setData(other.m_pData, other.m_dataLength);
}

Document::~Document()
{
	freeData();
}

Document& Document::operator=(const Document& other)
{
	if (this != &other)
	{
		// Copying does a deep copy
		DocumentInfo::operator=(other);
		setData(other.m_pData, other.m_dataLength);
		m_isMapped = false;
	}

	return *this;
}

bool Document::operator<(const Document& other) const
{
	if (DocumentInfo::operator<(other) == false)
	{
		if (m_dataLength < other.m_dataLength)
		{
			return true;
		}

		return false;
	}

	return true;
}

void Document::freeData(void)
{
	if (m_pData != NULL)
	{
		if (m_isMapped == false)
		{
			// Free
			free(m_pData);
		}
		else
		{
			// Unmap
			munmap((void*)m_pData, m_dataLength);
		}
	}

	m_pData = NULL;
	m_dataLength = 0;
	m_isMapped = false;
}

/// Copies the given data in the document.
bool Document::setData(const char *data, unsigned int length)
{
	if ((data == NULL) ||
		(length == 0))
	{
		return false;
	}

	// Discard existing data
	freeData();

	m_pData = (char *)malloc(sizeof(char) * (length + 1));
	if (m_pData != NULL)
	{
		memcpy(m_pData, data, length);
		m_pData[length] = '\0';
		m_dataLength = length;

		return true;
	}

	return false;
}

/// Maps the given file.
bool Document::setDataFromFile(const string &fileName)
{
	struct stat fileStat;

	if (fileName.empty() == true)
	{
		return false;
	}

	// Make sure the file exists
	if ((stat(fileName.c_str(), &fileStat) != 0) ||
		(!S_ISREG(fileStat.st_mode)))
	{
#ifdef DEBUG
		cout << "Document::setDataFromFile: " << fileName << " is not a file" << endl;
#endif
		return false;
	}

	if (fileStat.st_size == 0)
	{
		// The file is empty
		freeData();
		return true;
	}

	// Open the file in read-only mode
	int fd = open(fileName.c_str(), O_RDONLY);
	if (fd == -1)
	{
		cerr << "Document::setDataFromFile: " << fileName << " couldn't be opened" << endl;
		return false;
	}

	// Discard existing data
	freeData();

	// Request a private mapping of the whole file
	void *mapSpace = mmap(NULL, fileStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (mapSpace != MAP_FAILED)
	{
		m_pData = (char*)mapSpace;
		m_dataLength = fileStat.st_size;
		setTimestamp(TimeConverter::toTimestamp(fileStat.st_mtime));
		setSize(fileStat.st_size);
		m_isMapped = true;
	}
	else
	{
		cerr << "Document::setDataFromFile: mapping failed" << endl;
	}

	// Close the file
	if (close(fd) == -1)
	{
#ifdef DEBUG
		cout << "Document::setDataFromFile: close failed" << endl;
#endif
	}

	return m_isMapped;
}

/// Returns the document's data; NULL if document is empty.
const char *Document::getData(unsigned int &length) const
{
	length = m_dataLength;
	return m_pData;
}

/// Checks whether the document is binary.
bool Document::isBinary(void) const
{
	unsigned int maxLen = 100;

	// Look at the first 100 bytes or so
	if (m_dataLength < 100)
	{
		maxLen = m_dataLength;
	}
	for (unsigned int i = 0; i < maxLen; ++i)
	{
		if (isascii(m_pData[i]) == 0)
		{
#ifdef DEBUG
			cout << "Document::isBinary: " << m_pData[i] << endl;
#endif
			return true;
		}
	}

	return false;
}
