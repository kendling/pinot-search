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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <glibmm/thread.h>
#include <glibmm/convert.h>
#include <libxml++/parsers/domparser.h>
#include <libxml++/nodes/node.h>
#include <libxml++/nodes/textnode.h>

#include "StringManip.h"
#include "OpenSearchParser.h"

using namespace std;
using namespace Glib;
using namespace xmlpp;

static ustring getElementContent(const Element *pElem)
{
	if (pElem == NULL)
	{
		return "";
	}

#ifdef HAS_LIBXMLPP026
	const TextNode *pText = pElem->get_child_content();
#else
	const TextNode *pText = pElem->get_child_text();
#endif
	if (pText == NULL)
	{
		return "";
	}

	return pText->get_content();
}

OpenSearchResponseParser::OpenSearchResponseParser() :
	ResponseParserInterface()
{
}

OpenSearchResponseParser::~OpenSearchResponseParser()
{
}

bool OpenSearchResponseParser::parse(const ::Document *pResponseDoc, vector<Result> &resultsList,
	unsigned int maxResultsCount) const
{
}

OpenSearchParser::OpenSearchParser(const string &fileName) :
	PluginParserInterface(fileName)
{
}

OpenSearchParser::~OpenSearchParser()
{
}

ResponseParserInterface *OpenSearchParser::parse(SearchPluginProperties &properties,
	bool extractSearchParams)
{
	struct stat fileStat;
	bool success = true;

	if ((m_fileName.empty() == true) ||
		(stat(m_fileName.c_str(), &fileStat) != 0) ||
		(!S_ISREG(fileStat.st_mode)))
	{
		return NULL;
	}

	try
	{
		// Parse the configuration file
		DomParser parser;
		parser.set_substitute_entities(true);
		parser.parse_file(m_fileName);
		xmlpp::Document *pDocument = parser.get_document();
		if (pDocument == NULL)
		{
			return NULL;
		}

		Node *pNode = pDocument->get_root_node();
		Element *pRootElem = dynamic_cast<Element *>(pNode);
		if (pRootElem == NULL)
		{
			return NULL;
		}
		// Check the top-level element is what we expect
		ustring rootNodeName = pRootElem->get_name();
		if (rootNodeName != "OpenSearchDescription")
		{
#ifdef DEBUG
			cout << "OpenSearchParser::parse: wrong root node " << rootNodeName << endl;
#endif
			return NULL;
		}

		// Go through the subnodes
		const Node::NodeList childNodes = pRootElem->get_children();
		if (childNodes.empty() == false)
		{
			for (Node::NodeList::const_iterator iter = childNodes.begin(); iter != childNodes.end(); ++iter)
			{
				Node *pNode = (*iter);
				// All nodes should be elements
				Element *pElem = dynamic_cast<Element*>(pNode);
				if (pElem == NULL)
				{
					continue;
				}

				ustring nodeName = pElem->get_name();
				ustring nodeContent = getElementContent(pElem);
				if (nodeName == "ShortName")
				{
					properties.m_name = nodeContent;
				}
				else if (nodeName == "Description")
				{
					properties.m_description = nodeContent;
				}
				else if (nodeName == "Url")
				{
					ustring url, type;
					bool xmlResponse = false, getMethod = true;

					// Parse Query Syntax
					Element::AttributeList attributes = pElem->get_attributes();
					for (Element::AttributeList::const_iterator iter = attributes.begin();
						iter != attributes.end(); ++iter)
					{
						Attribute *pAttr = (*iter);

						if (pAttr != NULL)
						{
							ustring attrName = pAttr->get_name();
							ustring attrContent = pAttr->get_value();

							if (attrName == "template")
							{
								url = attrContent;
							}
							else if (attrName == "type")
							{
								type = attrContent;
							}
							else if (attrName == "method")
							{
								// GET is the default method
								if (StringManip::toLowerCase(attrContent) != "get")
								{
									getMethod = false;
								}
							}
						}
					}

					if (getMethod == true)
					{
						string::size_type startPos = 0, pos = url.find("?");

						// Determine if this Url is better than previous ones
						if ((type == "application/rss+xml") ||
							(type == "application/atom+xml"))
						{
							// We prefer OpenSearch Response
							xmlResponse = true;
#ifdef DEBUG
							cout << "OpenSearchParser::parse: XML response type" << endl;
#endif
						}
						else if (type == "text/html")
						{
							// HTML is second best
#ifdef DEBUG
							cout << "OpenSearchParser::parse: HTML response type" << endl;
#endif
							if (xmlResponse == true)
							{
								continue;
							}
						}
						else
						{
#ifdef DEBUG
							cout << "OpenSearchParser::parse: unsupported response type "
								<< type << endl;
#endif
							continue;
						}
	
						// Break the URL down into base and parameters
						if (pos != string::npos)
						{
							string params(url.substr(pos + 1));

							// URL
							properties.m_baseUrl = url.substr(0, pos);
#ifdef DEBUG
							cout << "OpenSearchParser::parse: URL is " << url << endl;
#endif

							// Split this into the actual parameters
							params += "&amp;";
							pos = params.find("&amp;");
							while (pos != string::npos)
							{
								string parameter(params.substr(startPos, pos - startPos));

								string::size_type equalPos = parameter.find("=");
								if (equalPos != string::npos)
								{
									string paramName(parameter.substr(0, equalPos));
									string paramValue(parameter.substr(equalPos + 1));
									SearchPluginProperties::Parameter param = SearchPluginProperties::UNKNOWN_PARAM;

									if (paramValue == "{searchTerms}")
									{
										param = SearchPluginProperties::SEARCH_TERMS_PARAM;
									}
									else if (paramValue == "{count}")
									{
										param = SearchPluginProperties::COUNT_PARAM;
									}
									else if (paramValue == "{startIndex}")
									{
										param = SearchPluginProperties::START_INDEX_PARAM;
									}
									else if (paramValue == "{startPage}")
									{
										param = SearchPluginProperties::START_PAGE_PARAM;
									}
									else if (paramValue == "{language}")
									{
										param = SearchPluginProperties::LANGUAGE_PARAM;
									}
									else if (paramValue == "{outputEncoding}")
									{
										param = SearchPluginProperties::OUTPUT_ENCODING_PARAM;
									}
									else if (paramValue == "{inputEncoding}")
									{
										param = SearchPluginProperties::INPUT_ENCODING_PARAM;
									}

									if (param != SearchPluginProperties::UNKNOWN_PARAM)
									{
										properties.m_parameters[param] = paramName;
									}
									else
									{
										// Append to the remainder
										if (properties.m_parametersRemainder.empty() == false)
										{
											properties.m_parametersRemainder += "&";
										}
										properties.m_parametersRemainder += paramName;
										properties.m_parametersRemainder += "=";
										properties.m_parametersRemainder += paramValue;
									}
								}

								// Next
								startPos = pos + 5;
								pos = params.find_first_of("&\n", startPos);
							}
						}

						// Method
						properties.m_method = SearchPluginProperties::GET_METHOD;
						// Output type
						properties.m_outputType = type;
						// Response
						if (xmlResponse == true)
						{
							properties.m_response = SearchPluginProperties::XML_RESPONSE;
						}
						else
						{
							properties.m_response = SearchPluginProperties::HTML_RESPONSE;
						}
					}

					// We ignore Param as we only support GET
				}
				else if (nodeName == "Tags")
				{
					// This is a space-delimited list, pick the first tag
					string::size_type pos = nodeContent.find(" ");
					properties.m_channel = nodeContent.substr(0, pos);
				}
				else if (nodeName == "LongName")
				{
					properties.m_longName = nodeContent;
				}
				else if (nodeName == "Language")
				{
					properties.m_languages.insert(nodeContent);
				}
				else if (nodeName == "OutputEncoding")
				{
					properties.m_outputEncodings.insert(nodeContent);
				}
				else if (nodeName == "InputEncoding")
				{
					properties.m_inputEncodings.insert(nodeContent);
				}
			}
		}
	}
	catch (const std::exception& ex)
	{
#ifdef DEBUG
		cout << "OpenSearchParser::parse: caught exception: " << ex.what() << endl;
#endif
		success = false;
	}

	if (success == false)
	{
		return NULL;
	}

	return new OpenSearchResponseParser();
}
