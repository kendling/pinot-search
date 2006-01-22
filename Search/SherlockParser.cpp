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

#include <stdlib.h>
#include <iostream>
#include <boost/spirit/core.hpp>
#include <boost/spirit/actor/push_back_actor.hpp>
#include <boost/spirit/actor/insert_at_actor.hpp>
#include <boost/spirit/utility/confix.hpp>

#include "StringManip.h"
#include "SherlockParser.h"

using std::cout;
using std::endl;
using std::string;
using std::map;

// A function object to lower case map keys with for_each()
struct LowerAndCopy
{
	public:
		LowerAndCopy(map<string, string> &other) :
			m_other(other)
		{
		}

		void operator()(map<string, string>::value_type &p)
		{
			m_other[StringManip::toLowerCase(p.first)] = p.second;
		}

		map<string, string> &m_other;

};

using namespace std;
using namespace boost::spirit;

struct skip_grammar : public grammar<skip_grammar>
{
	template <typename ScannerT>
	struct definition
	{
		definition(skip_grammar const &self)
		{
			// Skip all spaces and comments, starting with a #
			// FIXME: make sure comments start at the beginning of the line !
			skip = space_p | (ch_p('#') >> *(anychar_p - ch_p('\n')) >> ch_p('\n'));
		}
	
		rule<ScannerT> skip;
	
		rule<ScannerT> const& start() const
		{
			return skip;
		}
	};
};

/**
  * A minimal grammar for Sherlock plugins.
  * This only checks for the existence of the SEARCH tag.
  * It is used to quickly extract SEARCH attributes.
  */
struct plugin_min_grammar : public grammar<plugin_min_grammar>
{
	 plugin_min_grammar(map<string, string> &searchParams) :
		m_searchParams(searchParams)
	{
	}

	template <typename ScannerT>
	struct definition
	{
		definition(plugin_min_grammar const &self)
		{
			// Start
			search_plugin = search_header >> rest;

			// All items have a name and an optionally-quoted value, separated by =
			end_of_name = ch_p('=');
			any_name = *(~ch_p('>') - end_of_name);
			any_value_without_quotes = lexeme_d[*(~ch_p('>') - ch_p('\n'))];
			any_value = ch_p('\'') >> (*(~ch_p('\'')))[assign_a(unquotedValue)] >> ch_p('\'') |
				ch_p('"') >> (*(~ch_p('"')))[assign_a(unquotedValue)] >> ch_p('"') |
				any_value_without_quotes[assign_a(unquotedValue)];

			// SEARCH attributes are items
			// There should be only one SEARCH tag
			search_item = (any_name[assign_a(itemName)]
				>> ch_p('=') >> any_value[assign_a(itemValue, unquotedValue)])
				[insert_at_a(self.m_searchParams, itemName, itemValue)];

			// SEARCH may have any number of attributes
			search_header = ch_p('<') >> as_lower_d[str_p("search")] >> *search_item >> ch_p('>');

			// Rest
			rest = *anychar_p;
		}

		string unquotedValue, itemName, itemValue;		
		rule<ScannerT> search_plugin, search_header, rest;
		rule<ScannerT> end_of_name, any_name, any_value_without_quotes, any_value, search_item;

		rule<ScannerT> const& start() const
		{
			return search_plugin;
		}
	};

	map<string, string> &m_searchParams;

};

/**
  * A complete but lax grammar for Sherlock plugins.
  * For instance, it doesn't mind if INPUT has a NAME but no VALUE.
  * More importantly, it doesn't enforce types, eg FACTOR should be an integer.
  */
struct plugin_grammar : public grammar<plugin_grammar>
{
	 plugin_grammar(map<string, string> &searchParams,
		map<string, string> &interpretParams,
		map<string, string> &inputItems,
		string &userInput, string &nextInput,
		string &nextFactor,
		string &nextValue) :
		m_searchParams(searchParams),
		m_interpretParams(interpretParams),
		m_inputItems(inputItems),
		m_userInput(userInput),
		m_nextInput(nextInput),
		m_nextFactor(nextFactor),
		m_nextValue(nextValue)
	{
	}

	template <typename ScannerT>
	struct definition
	{
		definition(plugin_grammar const &self)
		{
			// Start
			search_plugin = search_header >> input_elements >> search_footer;

			// All items have a name and an optionally-quoted value, separated by =
			end_of_name = ch_p('=');
			any_name = *(~ch_p('>') - end_of_name);
			any_value_without_quotes = lexeme_d[*(~ch_p('>') - ch_p('\n'))];
			any_value = ch_p('\'') >> (*(~ch_p('\'')))[assign_a(unquotedValue)] >> ch_p('\'') |
				ch_p('"') >> (*(~ch_p('"')))[assign_a(unquotedValue)] >> ch_p('"') |
				any_value_without_quotes[assign_a(unquotedValue)];

			// SEARCH attributes are items
			// There should be only one SEARCH tag
			search_item = (any_name[assign_a(itemName)]
				>> ch_p('=') >> any_value[assign_a(itemValue, unquotedValue)])
				[insert_at_a(self.m_searchParams, itemName, itemValue)];

			// SEARCH may have any number of attributes
			search_header = ch_p('<') >> as_lower_d[str_p("search")] >> *search_item >> ch_p('>');

			// INPUT
			input_item_name = as_lower_d[str_p("name")] >> ch_p('=')
				>> any_value[assign_a(itemName, unquotedValue)];
			input_item_value = as_lower_d[str_p("value")] >> ch_p('=')
				>> any_value[assign_a(itemValue, unquotedValue)];
			input_item_user = as_lower_d[str_p("user")];
			input_item_factor = as_lower_d[str_p("factor")]
				>> ch_p('=') >> any_value[assign_a(itemValue, unquotedValue)];

			// INPUT tags have name and value items; one is marked with USER
			input_item = input_item_name |
				input_item_value |
				input_item_user[assign_a(self.m_userInput, itemName)];

			input_element = (ch_p('<') >> as_lower_d[str_p("input")] >> *input_item >> ch_p('>'))
				[insert_at_a(self.m_inputItems, itemName, itemValue)];

			// INPUTPREV tags have name and either factor or value items
			// There should be only one INPUTPREV tag
			// FIXME: save those
			inputprev_item = input_item_name |
				input_item_factor |
				input_item_value;

			inputprev_element = ch_p('<') >> as_lower_d[str_p("inputprev")] >> *inputprev_item >> ch_p('>');

			// INPUTNEXT tags have name and either factor or value items
			// There should be only one INPUTNEXT tag
			inputnext_item = input_item_name[assign_a(self.m_nextInput, itemName)] |
				input_item_factor[assign_a(self.m_nextFactor, itemValue)] |
				input_item_value[assign_a(self.m_nextValue, itemValue)];

			inputnext_element = ch_p('<') >> as_lower_d[str_p("inputnext")] >> *inputnext_item >> ch_p('>');

			// INTERPRET tags have varied types of items
			// There should be only one INTERPRET tag
			interpret_item = (any_name[assign_a(itemName)]
				>> ch_p('=') >> any_value[assign_a(itemValue, unquotedValue)])
				[insert_at_a(self.m_interpretParams, itemName, itemValue)];

			interpret_element = ch_p('<') >> as_lower_d[str_p("interpret")] >> *interpret_item >> ch_p('>');

			// INPUT, INPUTNEXT and INTERPRET may appear in any order
			input_elements = *(input_element |
				inputprev_element |
				inputnext_element |
				interpret_element);

			// SEARCH has a closing tag
			search_footer =  ch_p('<') >> ch_p('/') >> as_lower_d[str_p("search")] >> ch_p('>');
		}

		string unquotedValue, itemName, itemValue;		
		rule<ScannerT> search_plugin, search_header, search_footer;
		rule<ScannerT> end_of_name, any_name, any_value_without_quotes, any_value, search_item;
		rule<ScannerT> input_elements, input_element, inputprev_element, inputnext_element, interpret_element;
		rule<ScannerT> input_item_name, input_item_value, input_item_user, input_item_factor;
		rule<ScannerT> input_item, inputprev_item, inputnext_item, interpret_item;

		rule<ScannerT> const& start() const
		{
			return search_plugin;
		}
	};

	map<string, string> &m_searchParams;
	map<string, string> &m_interpretParams;
	map<string, string> &m_inputItems;
	string &m_userInput;
	string &m_nextInput;
	string &m_nextFactor;
	string &m_nextValue;

};

SherlockParser::SherlockParser(const Document *pDocument) :
	m_pDocument(pDocument)
{
}

SherlockParser::~SherlockParser()
{
}

bool SherlockParser::parse(bool extractSearchParams)
{
	if (m_pDocument == NULL)
	{
		return false;
	}

	unsigned int dataLength;
	const char *pData = m_pDocument->getData(dataLength);
	if ((pData == NULL) ||
		(dataLength == 0))
	{
		return false;
	}

	map<string, string> searchParams, interpretParams, inputItems;
	string userInput, nextInput, nextFactor, nextValue;
	skip_grammar skip;
	parse_info<> parseInfo;

	if (extractSearchParams == false)
	{
		plugin_grammar plugin(searchParams, interpretParams, inputItems,
			userInput, nextInput, nextFactor, nextValue);

		parseInfo = boost::spirit::parse(pData, plugin, skip);
	}
	else
	{
		plugin_min_grammar plugin(searchParams);

		parseInfo = boost::spirit::parse(pData, plugin, skip);
	}

	if (parseInfo.full == true)
	{
		map<string, string> lowSearchParams, lowInterpretParams, lowInputItems;

		LowerAndCopy lowCopy1(lowSearchParams);
		for_each(searchParams.begin(), searchParams.end(), lowCopy1);
		LowerAndCopy lowCopy2(lowInterpretParams);
		for_each(interpretParams.begin(), interpretParams.end(), lowCopy2);
		LowerAndCopy lowCopy3(lowInputItems);
		for_each(inputItems.begin(), inputItems.end(), lowCopy3);

		// Response
		m_properties.m_response = SearchPluginProperties::HTML_RESPONSE;
		// Method
		m_properties.m_method = SearchPluginProperties::GET_METHOD;

		// Name
		map<string, string>::iterator mapIter = lowSearchParams.find("name");
		if (mapIter != lowSearchParams.end())
		{
			m_properties.m_name = mapIter->second;
		}

		// Channel
		mapIter = lowSearchParams.find("routetype");
		if (mapIter != lowSearchParams.end())
		{
			m_properties.m_channel = mapIter->second;
		}

		if (extractSearchParams == false)
		{
			if (userInput.empty() == false)
			{
				// Remove the user input tag from the input tags map
				mapIter = lowInputItems.find(userInput);
				if (mapIter != lowInputItems.end())
				{
					lowInputItems.erase(mapIter);
				}

				m_properties.m_parameters[SearchPluginProperties::SEARCH_TERMS_PARAM] = userInput;
			}
			for (map<string, string>::iterator iter = lowInputItems.begin();
				iter != lowInputItems.end(); ++iter)
			{
				// Append to the remainder
				if (m_properties.m_parametersRemainder.empty() == false)
				{
					m_properties.m_parametersRemainder += "&";
				}
				m_properties.m_parametersRemainder += iter->first;
				m_properties.m_parametersRemainder += "=";
				m_properties.m_parametersRemainder += iter->second;
			}

			// URL
			mapIter = lowSearchParams.find("action");
			if (mapIter != lowSearchParams.end())
			{
				m_properties.m_baseUrl = mapIter->second;
			}

			// Response
			mapIter = lowInterpretParams.find("resultliststart");
			if (mapIter != lowInterpretParams.end())
			{
				m_properties.m_resultListStart = StringManip::replaceSubString(mapIter->second, "\\n", "\n");
			}

			mapIter = lowInterpretParams.find("resultlistend");
			if (mapIter != lowInterpretParams.end())
			{
				m_properties.m_resultListEnd = StringManip::replaceSubString(mapIter->second, "\\n", "\n");
			}

			mapIter = lowInterpretParams.find("resultitemstart");
			if (mapIter != lowInterpretParams.end())
			{
				m_properties.m_resultItemStart = StringManip::replaceSubString(mapIter->second, "\\n", "\n");
			}

			mapIter = lowInterpretParams.find("resultitemend");
			if (mapIter != lowInterpretParams.end())
			{
				m_properties.m_resultItemEnd = StringManip::replaceSubString(mapIter->second, "\\n", "\n");
			}

			mapIter = lowInterpretParams.find("resulttitlestart");
			if (mapIter != lowInterpretParams.end())
			{
				m_properties.m_resultTitleStart = mapIter->second;
			}

			mapIter = lowInterpretParams.find("resulttitleend");
			if (mapIter != lowInterpretParams.end())
			{
				m_properties.m_resultTitleEnd = mapIter->second;
			}

			mapIter = lowInterpretParams.find("resultlinkstart");
			if (mapIter != lowInterpretParams.end())
			{
				m_properties.m_resultLinkStart = mapIter->second;
			}

			mapIter = lowInterpretParams.find("resultlinkend");
			if (mapIter != lowInterpretParams.end())
			{
				m_properties.m_resultLinkEnd = mapIter->second;
			}

			mapIter = lowInterpretParams.find("resultextractstart");
			if (mapIter != lowInterpretParams.end())
			{
				m_properties.m_resultExtractStart = mapIter->second;
			}

			mapIter = lowInterpretParams.find("resultextractend");
			if (mapIter != lowInterpretParams.end())
			{
				m_properties.m_resultExtractEnd = mapIter->second;
			}

			mapIter = lowInterpretParams.find("skiplocal");
			if (mapIter != lowInterpretParams.end())
			{
				if (mapIter->second == "false")
				{
					m_properties.m_skipLocal = false;
				}
			}

			m_properties.m_nextTag = nextInput;
			// Here we differ from how Mozilla uses these parameters
			// Normally, either factor or value is used, but we use value
			// as the parameter's initial value
			if (nextFactor.empty() == false)
			{
				m_properties.m_nextFactor = (unsigned int)atoi(nextFactor.c_str());
			}
			if (nextValue.empty() == false)
			{
				m_properties.m_nextBase = (unsigned int)atoi(nextValue.c_str());
			}
		}
	}
#ifdef DEBUG
	else cout << "SherlockParser::parse: syntax error near " << parseInfo.stop << endl;
#endif

	return parseInfo.hit;
}

/// Returns the plugin's properties.
const SearchPluginProperties &SherlockParser::getProperties(void)
{
	return m_properties;
}
