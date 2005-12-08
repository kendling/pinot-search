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
#include <regex.h>
#include <stack>
#include <iostream>
#include <algorithm>

#include "StringManip.h"
#include "HtmlTokenizer.h"

//#define DEBUG_TOKENIZER

using namespace std;

/// Removes double and single quotes in links or any other attribute.
static string removeLinkQuotes(const string &quotedLink)
{
	string link;

	if (quotedLink[0] == '"')
	{
		string::size_type closingQuotePos = quotedLink.find("\"", 1);
		if (closingQuotePos != string::npos)
		{
			link = quotedLink.substr(1, closingQuotePos - 1);
		}
	}
	else if (quotedLink[0] == '\'')
	{
		string::size_type closingQuotePos = quotedLink.find("'", 1);
		if (closingQuotePos != string::npos)
		{
			link = quotedLink.substr(1, closingQuotePos - 1);
		}
	}
	else
	{
		// There are no quotes, so just look for the first space, if any
		string::size_type spacePos = quotedLink.find(" ");
		if (spacePos != string::npos)
		{
			link = quotedLink.substr(0, spacePos);
		}
		else
		{
			link = quotedLink;
		}
	}

	return link;
}

HtmlTokenizer::HtmlTokenizer(const Document *pDocument, unsigned int linksStartAtPos) :
	Tokenizer(NULL),
	m_pHtmlDocument(NULL),
	m_linkPos(linksStartAtPos)
{
	initialize(pDocument);
}

HtmlTokenizer::HtmlTokenizer() :
	Tokenizer(NULL),
	m_pHtmlDocument(NULL),
	m_linkPos(0)
{
}

HtmlTokenizer::~HtmlTokenizer()
{
	if (m_pDocument != NULL)
	{
		// This should have been set by setDocument(),
		// called in initialize()
		delete m_pDocument;
	}
}

void HtmlTokenizer::initialize(const Document *pDocument)
{
	unsigned int length = 0;

	if (pDocument == NULL)
	{
		return;
	}

	const char *data = pDocument->getData(length);
	if ((data != NULL) &&
		(length > 0))
	{
		// Remove HTML tags
		string strippedData = parseHTML(data);
		// Append META keywords, if any were found
		strippedData += getMetaTag("keywords");

		// Pass the result to the parent class
		Document *pStrippedDoc = new Document(pDocument->getTitle(),
			pDocument->getLocation(), pDocument->getType(),
			pDocument->getLanguage());
		pStrippedDoc->setData(strippedData.c_str(), strippedData.length());
		setDocument(pStrippedDoc);

		m_pHtmlDocument = pDocument;
	}
}

/// Parses HTML; the string without tags.
string HtmlTokenizer::parseHTML(const string &str, bool stripAllBlocks)
{
	stack<string> tagsStack;
	string stripped, link, linkName;
	string::size_type startPos = 0, linkOpenPos = 0;
	regex_t linksRegex, metaRegex;
	bool isHtml = false, skip = false, catText = stripAllBlocks;
	bool extractLinks = true, extractMetaTags = true, getLinkName = false;

#ifdef DEBUG_TOKENIZER
	cout << "HtmlTokenizer::parseHTML: start" << endl;
#endif
	// Prepare the regexps
	if (regcomp(&linksRegex, "a(.*)href=(.*)", REG_EXTENDED|REG_ICASE) != 0)
	{
#ifdef DEBUG_TOKENIZER
		cout << "HtmlTokenizer::parseHTML: couldn't compile links regexp" << endl;
#endif
		extractLinks = false;
	}
	if (regcomp(&metaRegex, "meta name=(.*) content=(.*)", REG_EXTENDED|REG_ICASE) != 0)
	{
#ifdef DEBUG_TOKENIZER
		cout << "HtmlTokenizer::parseHTML: couldn't compile meta tag regexp" << endl;
#endif
		extractMetaTags = false;
	}

	// Tag start
	string::size_type pos = str.find("<");
	while (pos != string::npos)
	{
		isHtml = true;

		if (skip == false)
		{
			string text = str.substr(startPos, pos - startPos);
			if (catText == true)
			{
				stripped += replaceEscapedCharacters(text);
			}

			// Is this part of the name of the last link we found ?
			if (getLinkName == true)
			{
				linkName += text;
#ifdef DEBUG_TOKENIZER
				cout << "HtmlTokenizer::parseHTML: adding to name " << text << endl;
#endif
			}

			startPos = pos + 1;
			// Tag end
			if (str[pos] == '<')
			{
				pos = str.find(">", startPos);
			}
			// Skip stuff in the tag
			skip = true;
		}
		else
		{
			regmatch_t pLinksMatches[3];
			regmatch_t pMetaMatches[3];
			int nLinksMatches = 3, nMetaMatches = 3;

			// Found a tag from startPos to pos
			string tag = str.substr(startPos, pos - startPos);
			// Lower case the whole thing
			tag = StringManip::toLowerCase(tag);
			// Push it onto the stack
			tagsStack.push(tag);
#ifdef DEBUG_TOKENIZER
			cout << "HtmlTokenizer::parseHTML: found " << tag << endl;
#endif

			if ((extractMetaTags == true) &&
				(regexec(&metaRegex, tag.c_str(), nMetaMatches, pMetaMatches, 
					REG_NOTBOL|REG_NOTEOL) == 0) &&
				(pLinksMatches[nMetaMatches - 1].rm_so != -1))
			{
				string tmp, metaName, metaContent;

				// META tag name
				tmp = tag.substr(pMetaMatches[1].rm_so,
					pMetaMatches[1].rm_eo - pMetaMatches[1].rm_so);
				// Remove quotes
				metaName = removeLinkQuotes(tmp);

				// META tag content
				tmp = tag.substr(pMetaMatches[2].rm_so,
					pMetaMatches[2].rm_eo - pMetaMatches[2].rm_so);
				// Remove quotes
				metaContent = removeLinkQuotes(tmp);
#ifdef DEBUG_TOKENIZER
				cout << "HtmlTokenizer::parseHTML: found META tag " << metaName << ": " << metaContent << endl;
#endif
				m_metaTags[metaName] = metaContent;
			}
			// See if this tag is an anchor
			// pLinksMatches[0] will be something like 'a href="blah"', pLinksMatches[1] will be ' ' and [2] will be '"blah"'
			else if ((extractLinks == true) &&
				(regexec(&linksRegex, tag.c_str(), nLinksMatches, pLinksMatches, REG_NOTBOL|REG_NOTEOL) == 0) &&
				(pLinksMatches[nLinksMatches - 1].rm_so != -1))
			{
				string quotedLink = tag.substr(pLinksMatches[2].rm_so,
					pLinksMatches[2].rm_eo - pLinksMatches[2].rm_so);

#ifdef DEBUG_TOKENIZER
				cout << "HtmlTokenizer::parseHTML: found link start " << tag << endl;
#endif
				if (link.empty() == false)
				{
					// The previous link's anchor's end couldn't be found ?
					m_links.insert(Link(stripTags(link), linkName, m_linkPos, linkOpenPos, startPos - 1));
					m_linkPos++;
					linkName = "";
#ifdef DEBUG_TOKENIZER
					cout << "HtmlTokenizer::parseHTML: previous link wasn't closed properly" << endl;
#endif
				}

				// Remove quotes
				link = removeLinkQuotes(quotedLink);
				linkOpenPos = startPos - 1;

				// Remember to get the name of the link
				getLinkName = true;
			}
			// Maybe it's the anchor's end ?
			else if ((extractLinks == true) &&
				(tag == "/a"))
			{
				if (getLinkName == true)
				{
#ifdef DEBUG_TOKENIZER
					cout << "HtmlTokenizer::parseHTML: " << pos << " link " << m_linkPos << " is " << link << ", name " << linkName << endl;
#endif
					// New link
					m_links.insert(Link(stripTags(link), linkName, m_linkPos, linkOpenPos, pos + 1));
					m_linkPos++;
					getLinkName = false;
					link = linkName = "";
				}
			}
			else if (stripAllBlocks == false)
			{
				if (textBlockStart(tag) == true)
				{
					catText = true;
#ifdef DEBUG_TOKENIZER
					cout << "HtmlTokenizer::parseHTML: start text cat" << endl;
#endif
				}
				else if (textBlockEnd(tag) == true)
				{
					catText = false;
#ifdef DEBUG_TOKENIZER
					cout << "HtmlTokenizer::parseHTML: end text cat" << endl;
#endif
				}
				else
				{
					string parentTag = tagsStack.top();

					if ((tag.substr(0, 6) == "script") ||
						(tag.substr(0, 5) == "style"))
					{
#ifdef DEBUG_TOKENIZER
						cout << "HtmlTokenizer::parseHTML: skip script" << endl;
#endif
						catText = false;
					}
					else if (((tag.substr(0, 7) == "/script") ||
						(tag.substr(0, 6) == "/style")) &&
						(textBlockStart(parentTag) == false))
					{
#ifdef DEBUG_TOKENIZER
						cout << "HtmlTokenizer::parseHTML: stop skip script" << endl;
#endif
						catText = true;
					}
				}
			}

			startPos = pos + 1;
			pos = str.find("<", startPos);
			skip = false;
		}
	}
	if ((startPos < str.length()) &&
		(catText == true))
	{
		stripped  += replaceEscapedCharacters(str.substr(startPos));
	}

	// Free the compiled regexps
	regfree(&linksRegex);
	regfree(&metaRegex);

	if (isHtml == false)
	{
		return str;
	}

	return stripped;
}

/// Returns true if the tag corresponds to a text block.
bool HtmlTokenizer::textBlockStart(const string &tag)
{
	if ((tag.substr(0, 4) == "body") ||
		(tag.substr(0, 5) == "title"))
	{
		return true;
	}

	return false;
}

/// Returns true if the tag corresponds to the end of a text block.
bool HtmlTokenizer::textBlockEnd(const string &tag)
{
	if ((tag.substr(0, 5) == "/body") ||
		(tag.substr(0, 6) == "/title"))
	{
		return true;
	}

	return false;
}

/// Replaces escaped characters
string HtmlTokenizer::replaceEscapedCharacters(const string &str)
{
	// FIXME: replace all escaped characters !
	static const char *escapedChars[] = { "quot", "amp", "lt", "gt", "nbsp", "eacute", "egrave", "agrave", "ccedil"};
	static const char *unescapedChars[] = { "\"", "&", "<", ">", " ", "e", "e", "a", "c"};
	static const unsigned int escapedCharsCount = 9;
	string unescaped;
	string::size_type startPos = 0, pos = 0;

#ifdef DEBUG_TOKENIZER
	cout << "HtmlTokenizer::replaceEscapedCharacters: input " << str << endl;
#endif
	pos = str.find("&");
	while (pos != string::npos)
	{
		unescaped += str.substr(startPos, pos - startPos);

		startPos = pos + 1;
		pos = str.find(";", startPos);
		if ((pos != string::npos) &&
			(pos < startPos + 10))
		{
			string escapedChar = str.substr(startPos, pos - startPos);
			bool replacedChar = false;

			// See if we can replace this with an actual character
			for (unsigned int count = 0; count < escapedCharsCount; ++count)
			{
				if (escapedChar == escapedChars[count])
				{
					unescaped += unescapedChars[count];
					replacedChar = true;
					break;
				}
			}

			if (replacedChar == false)
			{
				// This couldn't be replaced, leave it as it is...
				unescaped += "&";
				unescaped += escapedChar;
				unescaped += ";";
			}

			startPos = pos + 1;
		}

		// Next
		pos = str.find("&", startPos);
	}
	if (startPos < str.length())
	{
		unescaped  += str.substr(startPos);
	}
#ifdef DEBUG_TOKENIZER
	cout << "HtmlTokenizer::replaceEscapedCharacters: output " << unescaped << endl;
#endif

	return unescaped;
}

/// Gets the specified META tag content.
string HtmlTokenizer::getMetaTag(const string &name)
{
	if (name.empty() == true)
	{
		return "";
	}

	map<string, string>::const_iterator iter = m_metaTags.find(name);
	if (iter != m_metaTags.end())
	{
		return iter->second;
	}

	return "";
}

/// Gets the links map.
set<Link> &HtmlTokenizer::getLinks(void)
{
	return m_links;
}

/// Utility method that strips HTML tags off; the string without tags.
string HtmlTokenizer::stripTags(const string &str)
{
	HtmlTokenizer tokens;

	return tokens.parseHTML(str, true);
}

Link::Link(const string &url, const string &name, unsigned int pos, unsigned int openPos, unsigned int closePos) :
	m_url(url),
	m_name(name),
	m_pos(pos),
	m_open(openPos),
	m_close(closePos)
{
}

Link::Link(const Link &other) :
	m_url(other.m_url),
	m_name(other.m_name),
	m_pos(other.m_pos),
	m_open(other.m_open),
	m_close(other.m_close)
{
}

Link::~Link()
{
}

Link& Link::operator=(const Link& other)
{
	m_url = other.m_url;
	m_name = other.m_name;
	m_pos = other.m_pos;
	m_open = other.m_open;
	m_close = other.m_close;

	return *this;
}

bool Link::operator==(const Link &other) const
{
	return m_url == other.m_url;
}

bool Link::operator<(const Link &other) const
{
	return m_pos < other.m_pos;
}
