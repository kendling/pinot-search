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
#include <strings.h>
#include <sys/types.h>
#include <libxml/xmlerror.h>
#include <libxml/HTMLparser.h>
#include <iostream>

#include "StringManip.h"
#include "XmlTokenizer.h"
#include "HtmlTokenizer.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::map;
using std::set;

static bool getInBetweenLinksText(HtmlTokenizer::ParserState *pState,
	unsigned int currentLinkIndex)
{
	if (pState == NULL)
	{
		return false;
	}

	if ((pState->m_links.empty() == true) ||
		(pState->m_currentLink.m_index == 0))
	{
		string abstract(pState->m_text);

		StringManip::trimSpaces(abstract);

		pState->m_abstract = abstract;

		return true;
	}

	// Get the text between the current link and the previous one
	for (set<Link>::const_iterator linkIter = pState->m_links.begin();
		linkIter != pState->m_links.end(); ++linkIter)
	{
		// Is this the previous link ?
		if (linkIter->m_index == currentLinkIndex - 1)
		{
			// Is there text in between ?
			if (linkIter->m_endPos + 1 < pState->m_textPos)
			{
				unsigned int abstractLen = pState->m_textPos - linkIter->m_endPos - 1;
				string abstract(pState->m_text.substr(linkIter->m_endPos, abstractLen));

				StringManip::trimSpaces(abstract);

				// The longer, the better
				if (abstract.length() > pState->m_abstract.length())
				{
					pState->m_abstract = abstract;
#ifdef DEBUG
					cout << "HtmlTokenizer::getInBetweenLinksText: abstract after link "
						<< linkIter->m_index << endl;
#endif

					return true;
				}
			}

			break;
		}
	}

	return false;
}

static void startHandler(void *pData, const char *pElementName, const char **pAttributes)
{
	if ((pData == NULL) ||
		(pElementName == NULL) ||
		(strlen(pElementName) == 0))
	{
		return;
	}

	HtmlTokenizer::ParserState *pState = (HtmlTokenizer::ParserState *)pData;
	if (pState == NULL)
	{
		return;
	}

	// Reset the text hash
	pState->m_lastHash.clear();

	// What tag is this ?
	string tagName(StringManip::toLowerCase(pElementName));
	if ((pState->m_foundHead == false) &&
		(tagName == "head"))
	{
		// Expect to find META tags and a title
		pState->m_inHead = true;
		// One head is enough :-)
		pState->m_foundHead = true;
	}
	else if ((pState->m_inHead == true) &&
		(tagName == "meta") &&
		(pAttributes != NULL))
	{
		string metaName, metaContent;

		// Get the META tag's name and content
		for (unsigned int attrNum = 0;
			(pAttributes[attrNum] != NULL) && (pAttributes[attrNum + 1]); attrNum += 2)
		{
			if (strncasecmp(pAttributes[attrNum], "name", 4) == 0)
			{
				metaName = pAttributes[attrNum + 1];
			}
			else if (strncasecmp(pAttributes[attrNum], "content", 7) == 0)
			{
				metaContent = pAttributes[attrNum + 1];
			}
			else if (strncasecmp(pAttributes[attrNum], "http-equiv", 10) == 0)
			{
				metaName = pAttributes[attrNum + 1];
			}
		}

		if ((metaName.empty() == false) &&
			(metaContent.empty() == false))
		{
			// Store this META tag
			pState->m_metaTags[StringManip::toLowerCase(metaName)] = metaContent;
		}
	}
	else if ((pState->m_inHead == true) &&
		(tagName == "title"))
	{
		// Extract title
		pState->m_appendToTitle = true;
	}
	else if (tagName == "body")
	{
		// Index text
		pState->m_appendToText = true;
	}
	else if ((tagName == "a") &&
		(pAttributes != NULL))
	{
		pState->m_currentLink.m_url.clear();
		pState->m_currentLink.m_name.clear();

		// Get the href
		for (unsigned int attrNum = 0;
			(pAttributes[attrNum] != NULL) && (pAttributes[attrNum + 1]); attrNum += 2)
		{
			if (strncasecmp(pAttributes[attrNum], "href", 4) == 0)
			{
				pState->m_currentLink.m_url = pAttributes[attrNum + 1];
				break;
			}
		}

		if (pState->m_currentLink.m_url.empty() == false)
		{
			// FIXME: get the NodeInfo to find out the position of this link
			pState->m_currentLink.m_startPos = pState->m_textPos;

			// Find abstract ?
			if (pState->m_findAbstract == true)
			{
				getInBetweenLinksText(pState, pState->m_currentLink.m_index);
			}

			// Extract link
			pState->m_appendToLink = true;
		}
	}
	else if ((tagName == "frame") &&
		(pAttributes != NULL))
	{
		Link frame;

		// Get the name and source
		for (unsigned int attrNum = 0;
			(pAttributes[attrNum] != NULL) && (pAttributes[attrNum + 1]); attrNum += 2)
		{
			if (strncasecmp(pAttributes[attrNum], "name", 4) == 0)
			{
				frame.m_name = pAttributes[attrNum + 1];
			}
			else if (strncasecmp(pAttributes[attrNum], "src", 3) == 0)
			{
				frame.m_url = pAttributes[attrNum + 1];
			}
		}

		if (frame.m_url.empty() == false)
		{
			// Store this frame
			pState->m_frames.insert(frame);
		}
	}
	else if ((tagName == "frameset") ||
		(tagName == "script") ||
		(tagName == "style"))
	{
		// Skip
		++pState->m_skip;
	}
}

static void endHandler(void *pData, const char *pElementName)
{
	if ((pData == NULL) ||
		(pElementName == NULL) ||
		(strlen(pElementName) == 0))
	{
		return;
	}

	HtmlTokenizer::ParserState *pState = (HtmlTokenizer::ParserState *)pData;
	if (pState == NULL)
	{
		return;
	}

	// Reset state
	string tagName(StringManip::toLowerCase(pElementName));
	if (tagName == "head")
	{
		pState->m_inHead = false;
	}
	else if (tagName == "title")
	{
		StringManip::trimSpaces(pState->m_title);
		StringManip::removeCharacters(pState->m_title, "\r\n");
#ifdef DEBUG
		cout << "HtmlTokenizer::endHandler: title is " << pState->m_title << endl;
#endif
		pState->m_appendToTitle = false;
	}
	else if (tagName == "body")
	{
		pState->m_appendToText = false;
	}
	else if (tagName == "a")
	{
		if (pState->m_currentLink.m_url.empty() == false)
		{
			StringManip::trimSpaces(pState->m_currentLink.m_name);
			StringManip::removeCharacters(pState->m_currentLink.m_name, "\r\n");

			pState->m_currentLink.m_endPos = pState->m_textPos;

			// Store this link
			pState->m_links.insert(pState->m_currentLink);
			++pState->m_currentLink.m_index;
		}

		pState->m_appendToLink = false;
	}
	else if ((tagName == "frameset") ||
		(tagName == "script") ||
		(tagName == "style"))
	{
		--pState->m_skip;
	}
}

static void charactersHandler(void *pData, const char *pText, int textLen)
{
	if ((pData == NULL) ||
		(pText == NULL) ||
		(textLen == 0))
	{
		return;
	}

	HtmlTokenizer::ParserState *pState = (HtmlTokenizer::ParserState *)pData;
	if (pState == NULL)
	{
		return;
	}

	if (pState->m_skip > 0)
	{
		// Skip this
		return;
	}

	string text(pText, textLen);

	// For some reason, this handler might be called twice for the same text !
	// See http://mail.gnome.org/archives/xml/2002-September/msg00089.html
	string textHash(StringManip::hashString(text));
	if (pState->m_lastHash == textHash)
	{
		// Ignore this
		return;
	}
	pState->m_lastHash = textHash;

	// Append current text
	// FIXME: convert to UTF-8 or Latin 1 ?
	if (pState->m_appendToTitle == true)
	{
		pState->m_title += text;
	}
	else
	{
		if (pState->m_appendToText == true)
		{
			pState->m_text += text;
			pState->m_textPos += textLen;
		}

		// Appending to text and to link are not mutually exclusive operations
		if (pState->m_appendToLink == true)
		{
			pState->m_currentLink.m_name += text;
		}
	}
}

static void cDataHandler(void *pData, const char *pText, int textLen)
{
	// Nothing to do
}

static void whitespaceHandler(void *pData, const xmlChar *pText, int txtLen)
{
	if (pData == NULL)
	{
		return;
	}

	HtmlTokenizer::ParserState *pState = (HtmlTokenizer::ParserState *)pData;
	if (pState == NULL)
	{
		return;
	}

	if (pState->m_skip > 0)
	{
		// Skip this
		return;
	}

	// Append a single space
	if (pState->m_appendToTitle == true)
	{
		pState->m_title += " ";
	}
	else
	{
		if (pState->m_appendToText == true)
		{
			pState->m_text += " ";
		}

		// Appending to text and to link are not mutually exclusive operations
		if (pState->m_appendToLink == true)
		{
			pState->m_currentLink.m_name += " ";
		}
	}
}

static void commentHandler(void *pData, const char *pText)
{
	// FIXME: take comments into account, eg on terms position ?
}

static void errorHandler(void *pData, const char *pMsg, ...)
{
	if (pData == NULL)
	{
		return;
	}

	HtmlTokenizer::ParserState *pState = (HtmlTokenizer::ParserState *)pData;
	if (pState == NULL)
	{
		return;
	}

	va_list args;
	char pErr[1000];

	va_start(args, pMsg);
	vsnprintf(pErr, 1000, pMsg, args );
	va_end(args);

	cerr << "HtmlTokenizer::errorHandler: " << pErr << endl;

	// Be lenient as much as possible
	xmlResetLastError();
	// ...but remember the document had errors
	pState->m_isValid = false;
}

static void warningHandler(void *pData, const char *pMsg, ...)
{
	va_list args;
	char pErr[1000];

	va_start(args, pMsg);
	vsnprintf(pErr, 1000, pMsg, args );
	va_end(args);

	cerr << "HtmlTokenizer::warningHandler: " << pErr << endl;
}

Link::Link() :
	m_index(0),
	m_startPos(0),
	m_endPos(0)
{
}

Link::Link(const Link &other) :
	m_url(other.m_url),
	m_name(other.m_name),
	m_index(other.m_index),
	m_startPos(other.m_startPos),
	m_endPos(other.m_endPos)
{
}

Link::~Link()
{
}

Link& Link::operator=(const Link& other)
{
	m_url = other.m_url;
	m_name = other.m_name;
	m_index = other.m_index;
	m_startPos = other.m_startPos;
	m_endPos = other.m_endPos;

	return *this;
}

bool Link::operator==(const Link &other) const
{
	return m_url == other.m_url;
}

bool Link::operator<(const Link &other) const
{
	return m_index < other.m_index;
}

HtmlTokenizer::ParserState::ParserState() :
	m_isValid(true),
	m_findAbstract(false),
	m_textPos(0),
	m_inHead(false),
	m_foundHead(false),
	m_appendToTitle(false),
	m_appendToText(false),
	m_appendToLink(false),
	m_skip(0)
{
}

HtmlTokenizer::ParserState::~ParserState()
{
}

HtmlTokenizer::HtmlTokenizer(const Document *pDocument,
	bool validateOnly, bool findAbstract) :
	Tokenizer(NULL)
{
	if (validateOnly == true)
	{
		// This will ensure text is skipped
		++m_state.m_skip;
	}
	else
	{
		// Attempt to find an abstract ?
		m_state.m_findAbstract = findAbstract;
	}

	if (parseHTML(pDocument) == true)
	{
		// Did we find a title ?
		if (m_state.m_title.empty() == true)
		{
			m_state.m_title = pDocument->getTitle();
		}

		// Pass the result to the parent class
		Document *pStrippedDoc = new Document(m_state.m_title,
			pDocument->getLocation(), pDocument->getType(),
			pDocument->getLanguage());
		pStrippedDoc->setData(m_state.m_text.c_str(), m_state.m_text.length());

		setDocument(pStrippedDoc);
	}
}

HtmlTokenizer::~HtmlTokenizer()
{
	if (m_pDocument != NULL)
	{
		// This should have been set by setDocument()
		delete m_pDocument;
	}
}

bool HtmlTokenizer::parseHTML(const Document *pDocument)
{
	if (pDocument == NULL)
	{
		return false;
	}

	unsigned int dataLength = 0;
	const char *pData = pDocument->getData(dataLength);
	if ((pData == NULL) ||
		(dataLength == 0))
	{
#ifdef DEBUG
		cout << "HtmlTokenizer::parseHTML: no input" << endl;
#endif
		return false;
	}

	string htmlChunk(pData, dataLength);
	htmlSAXHandler saxHandler;

	xmlInitParser();

	// Setup the SAX handler
	memset((void*)&saxHandler, 0, sizeof(htmlSAXHandler));
	saxHandler.startElement = (startElementSAXFunc)&startHandler;
	saxHandler.endElement = (endElementSAXFunc)&endHandler;
	saxHandler.characters = (charactersSAXFunc)&charactersHandler;
	saxHandler.cdataBlock = (charactersSAXFunc)&cDataHandler;
	saxHandler.ignorableWhitespace = (ignorableWhitespaceSAXFunc)&whitespaceHandler;
	saxHandler.comment = (commentSAXFunc)&commentHandler;
	saxHandler.fatalError = (fatalErrorSAXFunc)&errorHandler;
	saxHandler.error = (errorSAXFunc)&errorHandler;
	saxHandler.warning = (warningSAXFunc)&warningHandler;

	// Try to cope with pages that have scripts or other rubbish prepended
	string::size_type htmlPos = htmlChunk.find("<!DOCTYPE");
	if (htmlPos == string::npos)
	{
		htmlPos = htmlChunk.find("<!doctype");
	}
	if ((htmlPos != string::npos) &&
		(htmlPos > 0))
	{
		htmlChunk.erase(0, htmlPos);
#ifdef DEBUG
		cout << "HtmlTokenizer::parseHTML: removed " << htmlPos << " characters" << endl;
#endif
	}

#ifndef _DONT_USE_PUSH_INTERFACE
	htmlParserCtxtPtr pContext = htmlCreatePushParserCtxt(&saxHandler, (void*)&m_state,
		htmlChunk.c_str(), (int)htmlChunk.length(), "", XML_CHAR_ENCODING_NONE);
	if (pContext != NULL)
	{
		// Parse
		htmlParseChunk(pContext, htmlChunk.c_str(), (int)htmlChunk.length(), 0);

		// Free
		htmlParseChunk(pContext, NULL, 0, 1);
		htmlFreeParserCtxt(pContext);
	}
#else
	htmlDocPtr pDoc = htmlSAXParseDoc((xmlChar *)htmlChunk.c_str(), "", &saxHandler, (void*)&m_state);
	if (pDoc != NULL)
	{
		xmlFreeDoc(pDoc);
	}
#endif
	else
	{
		cerr << "HtmlTokenizer::parseHTML: couldn't create parser context" << endl;
	}
	xmlCleanupParser();

	// The text after the last link might make a good abstract
	if (m_state.m_findAbstract == true)
	{
		getInBetweenLinksText(&m_state, m_state.m_currentLink.m_index);
	}

	// Append META keywords, if any were found
	m_state.m_text += getMetaTag("keywords");

	return true;
}

/// Determines whether the document is properly formed.
bool HtmlTokenizer::isDocumentValid(void) const
{
	return m_state.m_isValid;
}

/// Gets the specified META tag content.
string HtmlTokenizer::getMetaTag(const string &name) const
{
	if (name.empty() == true)
	{
		return "";
	}

	map<string, string>::const_iterator iter = m_state.m_metaTags.find(StringManip::toLowerCase(name));
	if (iter != m_state.m_metaTags.end())
	{
		return iter->second;
	}

	return "";
}

/// Gets the links map.
set<Link> &HtmlTokenizer::getLinks(void)
{
	return m_state.m_links;
}

/// Gets the abstract.
std::string HtmlTokenizer::getAbstract(void) const
{
	return m_state.m_abstract;
}
