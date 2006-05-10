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

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <regex.h>
#include <iostream>
#include <algorithm>

#include <gmime/gmime.h>

#include "StringManip.h"
#include "TimeConverter.h"
#include "Url.h"
#include "MboxParser.h"

using std::cout;
using std::endl;

MboxParser::MboxParser(const string &fileName, off_t mboxOffset)
{
	m_fileName = fileName;
	m_fd = -1;
	m_pMboxStream = NULL;
	m_pParser = NULL;
	m_partsCount = m_partNum = -1;
	m_messageStart = mboxOffset;
	m_pCurrentMessage = NULL;
	m_messageDate = 0;

	if (initialize() == true)
	{
		// Extract the first message
		extractMessage();
	}
}

MboxParser::~MboxParser()
{
	if (m_pCurrentMessage != NULL)
	{
		delete m_pCurrentMessage;
	}

	finalize();
}

bool MboxParser::initialize(void)
{
	// Open the mbox file
	m_fd = open(m_fileName.c_str(), O_RDONLY);
	if (m_fd < 0)
	{
		return false;
	}

	// Initialize gmime
	g_mime_init(GMIME_INIT_FLAG_UTF8);

	// Create a stream
	if (m_messageStart > 0)
	{
		struct stat fileStat;

		if ((fstat(m_fd, &fileStat) == 0) &&
			(!S_ISREG(fileStat.st_mode)))
		{
			// This is not a file !
			return false;
		}

		if (m_messageStart > fileStat.st_size)
		{
			// This offset doesn't make sense !
			m_messageStart = 0;
		}

		m_pMboxStream = g_mime_stream_fs_new_with_bounds(m_fd, m_messageStart, fileStat.st_size);
#ifdef DEBUG
		cout << "MboxParser::initialize: stream starts at offset " << m_messageStart << endl;
#endif
	}
	else
	{
		m_pMboxStream = g_mime_stream_fs_new(m_fd);
	}

	// And a parser
	m_pParser = g_mime_parser_new();
	if ((m_pMboxStream != NULL) &&
		(m_pParser != NULL))
	{
		g_mime_parser_init_with_stream(m_pParser, m_pMboxStream);
		g_mime_parser_set_respect_content_length(m_pParser, TRUE);
		// Scan for mbox From-lines
		g_mime_parser_set_scan_from(m_pParser, TRUE);

		return true;
	}

	return false;
}

void MboxParser::finalize(void)
{
	if (m_pParser != NULL)
	{
		// FIXME: does the parser close the stream ?
		g_object_unref(G_OBJECT(m_pParser));
	}
	else if (m_pMboxStream != NULL)
	{
		g_object_unref(G_OBJECT(m_pMboxStream));
	}
	if (m_fd >= 0)
	{
		close(m_fd);
	}
	g_mime_shutdown();
}

char *MboxParser::extractPart(GMimeObject *part, string &contentType, ssize_t &partLen)
{
	char *pBuffer = NULL;

	if (part == NULL)
	{
		return NULL;
	}

	// Message parts may be nested
	while (GMIME_IS_MESSAGE_PART(part))
	{
#ifdef DEBUG
		cout << "MboxParser::extractPart: nested message part" << endl;
#endif
		GMimeMessage *partMessage = g_mime_message_part_get_message(GMIME_MESSAGE_PART(part));
		part = g_mime_message_get_mime_part(partMessage);
		g_mime_object_unref(GMIME_OBJECT(partMessage));
	}

	// Is this a multipart ?
	if (GMIME_IS_MULTIPART(part))
	{
		m_partsCount = g_mime_multipart_get_number(GMIME_MULTIPART(part));
#ifdef DEBUG
		cout << "MboxParser::extractPart: message has " << m_partsCount << " parts" << endl;
#endif
		for (int partNum = max(m_partNum, 0); partNum < m_partsCount; ++partNum)
		{
#ifdef DEBUG
			cout << "MboxParser::extractPart: extracting part " << partNum << endl;
#endif
			
			GMimeObject *multiMimePart = g_mime_multipart_get_part(GMIME_MULTIPART(part), partNum);
			if (multiMimePart == NULL)
			{
				continue;
			}

			char *pPart = extractPart(multiMimePart, contentType, partLen);
			g_mime_object_unref(multiMimePart);
			if (pPart != NULL)
			{
				m_partNum = ++partNum;
				return NULL;
			}
		}

		// None of the parts were suitable
		m_partsCount = m_partNum = -1;
	}

	if (!GMIME_IS_PART(part))
	{
#ifdef DEBUG
		cout << "MboxParser::extractPart: not a part" << endl;
#endif
		return NULL;
	}
	GMimePart *mimePart = GMIME_PART(part);

	// Check the content type
	const GMimeContentType *mimeType = g_mime_part_get_content_type(mimePart);
	if (g_mime_content_type_is_type(mimeType, "text", "*") == TRUE)
	{
		// Set this for caller
		char *partType = g_mime_content_type_to_string(mimeType);
		if (partType != NULL)
		{
			contentType = partType;
			g_free(partType);
		}

		GMimePartEncodingType encodingType = g_mime_part_get_encoding(mimePart);
#ifdef DEBUG
		cout << "MboxParser::extractPart: encoding is " << encodingType << endl;
#endif

		// Write the part to memory
		g_mime_part_set_encoding(mimePart, GMIME_PART_ENCODING_QUOTEDPRINTABLE);
		GMimeStream *memStream = g_mime_stream_mem_new();
		GMimeDataWrapper *dataWrapper = g_mime_part_get_content_object(mimePart);
		if (dataWrapper != NULL)
		{
			ssize_t writeLen = g_mime_data_wrapper_write_to_stream(dataWrapper, memStream);
#ifdef DEBUG
			cout << "MboxParser::extractPart: wrote " << writeLen << " bytes" << endl;
#endif
			g_object_unref(dataWrapper);
		}
		g_mime_stream_flush(memStream);
		partLen = g_mime_stream_length(memStream);
#ifdef DEBUG
		cout << "MboxParser::extractPart: part is " << partLen << " bytes long" << endl;
#endif

		pBuffer = (char*)malloc(partLen + 1);
		pBuffer[partLen] = '\0';
		g_mime_stream_reset(memStream);
		ssize_t readLen = g_mime_stream_read(memStream, pBuffer, partLen);
#ifdef DEBUG
		cout << "MboxParser::extractPart: read " << readLen << " bytes" << endl;
#endif
		g_mime_stream_unref(memStream);
	}
#ifdef DEBUG
	else	cout << "MboxParser::extractPart: part is not text" << endl;
#endif

	return pBuffer;
}

bool MboxParser::extractMessage(void)
{
	string fromLine, contentType;

	while (g_mime_stream_eos(m_pMboxStream) == FALSE)
	{
		if (m_partsCount != -1)
		{
			// FIXME: the previous message has parts left to parse
			// The parser doesn't allow to poke at the stream
			// See g_mime_parser_init_with_stream() here :
			// http://spruce.sourceforge.net/gmime/doc/gmime-gmime-parser.html
#ifdef DEBUG
			cout << "MboxParser::extractMessage: skipping parts..." << endl;
#endif
			m_partsCount = m_partNum = -1;
		}
		// Get the next message
		GMimeMessage *mimeMessage = g_mime_parser_construct_message(m_pParser);

		m_messageStart = g_mime_parser_get_from_offset(m_pParser);
		off_t messageEnd = g_mime_parser_tell(m_pParser);

#ifdef DEBUG
		cout << "MboxParser::extractMessage: message between offsets " << m_messageStart
			<< " and " << messageEnd << endl;
#endif
		if (messageEnd > m_messageStart)
		{
			char *msgFromLine = g_mime_parser_get_from(m_pParser);
			if (msgFromLine != NULL)
			{
				fromLine = msgFromLine;
				g_free(msgFromLine);
			}

			// FIXME: this only applies to Mozilla
			const char *msgMozStatus = g_mime_message_get_header(mimeMessage, "X-Mozilla-Status");
			if (msgMozStatus != NULL)
			{
				long int mozStatus = strtol(msgMozStatus, NULL, 16);
				// Watch out for Mozilla specific flags :
				// MSG_FLAG_EXPUNGED, MSG_FLAG_EXPIRED
				// They are defined in mailnews/MailNewsTypes.h and msgbase/nsMsgMessageFlags.h
				if ((mozStatus & 0x0008) ||
					(mozStatus & 0x0040))
				{
#ifdef DEBUG
					cout << "MboxParser::extractMessage: flagged by Mozilla" << endl;
#endif
					g_mime_object_unref(GMIME_OBJECT(mimeMessage));
					continue;
				}
			}

			// How old is this message ?
			const char *msgDate = g_mime_message_get_header(mimeMessage, "Date");
			if (msgDate != NULL)
			{
				m_messageDate = TimeConverter::fromTimestamp(msgDate);
			}
			else
			{
				m_messageDate = 0;
			}
#ifdef DEBUG
			cout << "MboxParser::extractMessage: message date is " << m_messageDate << endl;
#endif

			// Extract the subject and source address
			const char *msgSubject = g_mime_message_get_header(mimeMessage, "Subject");
			const char *msgFrom = g_mime_message_get_header(mimeMessage, "From");

			// Get the top-level MIME part in the message
			GMimeObject *mimePart = g_mime_message_get_mime_part(mimeMessage);

			// Extract the part's text
			ssize_t partLength = 0;
			char *pPart = extractPart(mimePart, contentType, partLength);
			if (pPart != NULL)
			{
				string location, subject;
				char posStr[64];

				// New location
				// FIXME: use the same scheme as Mozilla
				location = "mailbox://";
				location += m_fileName;
				location += "?o=";
				snprintf(posStr, 64, "%d", m_messageStart);
				location += posStr;
				location += "&p=";
				snprintf(posStr, 64, "%d", max(m_partNum, 0));
				location += posStr;
				location += "&h=";
				location += StringManip::hashString(fromLine);
#ifdef DEBUG
				cout << "MboxParser::extractMessage: message location is " << location << endl;
#endif

				if (msgSubject != NULL)
				{
					subject = msgSubject;
#ifdef DEBUG
					cout << "MboxParser::extractMessage: message subject is " << subject << endl;
#endif
				}
#ifdef DEBUG
				else cout << "MboxParser::extractMessage: message has no subject" << endl;
#endif

				// New message
				m_pCurrentMessage = new Document(subject, location, contentType, "");
				m_pCurrentMessage->setData(pPart, (unsigned int)partLength);

				free(pPart);
				g_mime_object_unref(mimePart);
				g_mime_object_unref(GMIME_OBJECT(mimeMessage));

				return true;
			}

			g_mime_object_unref(mimePart);
		}

		g_mime_object_unref(GMIME_OBJECT(mimeMessage));
	}

	return false;
}

/// Gets the current message's date.
time_t MboxParser::getDate(void) const
{
	return m_messageDate;
}

/// Jumps to the next message.
bool MboxParser::nextMessage(void)
{
	delete m_pCurrentMessage;
	m_pCurrentMessage = NULL;
	// Get the next message from the original document
	return extractMessage();
}

/// Returns a pointer to the current message's document.
const Document *MboxParser::getDocument(void)
{
	// FIXME: this object can be invalidated by nextMessage() at any time;
	// use auto_ptr all the way ?
	return m_pCurrentMessage;
}
