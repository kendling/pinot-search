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

#ifndef _MBOX_PARSER_H
#define _MBOX_PARSER_H

#include <time.h>
#include <string>
#include <map>
#include <utility>
#include <set>

#include <gmime/gmime-object.h>
#include <gmime/gmime-stream.h>
#include <gmime/gmime-parser.h>

#include "Document.h"

using namespace std;

/**
  * A parser for mbox files. Each message is extracted and
  * returned in a separate document by getDocument(). The value
  * returned by getDocumentIncrement() enables to determine a new
  * message has been extracted and is ready to be tokenized.
  * See http://en.wikipedia.org/wiki/Mbox for details about the format.
  */
class MboxParser
{
	public:
		MboxParser(const string &fileName, off_t mboxOffset = 0);
		virtual ~MboxParser();

		/// Gets the current message's date.
		time_t getDate(void) const;

		/// Jumps to the next message.
		bool nextMessage(void);

		/// Returns a pointer to the current message's document.
		virtual const Document *getDocument(void);

	protected:
		string m_fileName;
		int m_fd;
		GMimeStream *m_pMboxStream;
		GMimeParser *m_pParser;
		int m_partsCount;
		int m_partNum;
		off_t m_messageStart;
		Document *m_pCurrentMessage;
		time_t m_messageDate;

		bool initialize(void);

		void finalize(void);

		bool extractMessage(void);

		char *extractPart(GMimeObject *mimeObject, string &contentType, ssize_t &partLen);

	private:
		MboxParser(const MboxParser &other);
		MboxParser& operator=(const MboxParser& other);

};

#endif // _MBOX_PARSER_H
