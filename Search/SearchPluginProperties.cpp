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

#include "SearchPluginProperties.h"

using std::string;
using std::map;
using std::set;

SearchPluginProperties::SearchPluginProperties() :
	m_method(GET_METHOD),
	m_nextFactor(10),
	m_nextBase(0),
	m_response(UNKNOWN_RESPONSE),
	m_skipLocal(true)
{
}

SearchPluginProperties::SearchPluginProperties(const SearchPluginProperties &other) :
	m_name(other.m_name),
	m_longName(other.m_longName),
	m_description(other.m_description),
	m_channel(other.m_channel),
	m_baseUrl(other.m_baseUrl),
	m_method(other.m_method),
	m_parametersRemainder(other.m_parametersRemainder),
	m_outputType(other.m_outputType),
	m_nextTag(other.m_nextTag),
	m_nextFactor(other.m_nextFactor),
	m_nextBase(other.m_nextBase),
	m_resultListStart(other.m_resultListStart),
	m_resultListEnd(other.m_resultListEnd),
	m_resultItemStart(other.m_resultItemStart),
	m_resultItemEnd(other.m_resultItemEnd),
	m_resultTitleStart(other.m_resultTitleStart),
	m_resultTitleEnd(other.m_resultTitleEnd),
	m_resultLinkStart(other.m_resultLinkStart),
	m_resultLinkEnd(other.m_resultLinkEnd),
	m_resultExtractStart(other.m_resultExtractStart),
	m_resultExtractEnd(other.m_resultExtractEnd),
	m_skipLocal(other.m_skipLocal)
{
	copy(other.m_languages.begin(), other.m_languages.end(),
		inserter(m_languages, m_languages.begin()));
	copy(other.m_outputEncodings.begin(), other.m_outputEncodings.end(),
		inserter(m_outputEncodings, m_outputEncodings.begin()));
	copy(other.m_inputEncodings.begin(), other.m_inputEncodings.end(),
		inserter(m_inputEncodings, m_inputEncodings.begin()));
	copy(other.m_parameters.begin(), other.m_parameters.end(),
		inserter(m_parameters, m_parameters.begin()));
}

SearchPluginProperties::~SearchPluginProperties()
{
}

SearchPluginProperties& SearchPluginProperties::operator=(const SearchPluginProperties& other)
{
	m_name = other.m_name;
	m_longName = other.m_longName;
	m_description = other.m_description;
	m_channel = other.m_channel;
	m_baseUrl = other.m_baseUrl;
	m_method = other.m_method;
	m_parametersRemainder = other.m_parametersRemainder;
	m_outputType = other.m_outputType;
	m_nextTag = other.m_nextTag;
	m_nextFactor = other.m_nextFactor;
	m_nextBase = other.m_nextBase;
	m_resultListStart = other.m_resultListStart;
	m_resultListEnd = other.m_resultListEnd;
	m_resultItemStart = other.m_resultItemStart;
	m_resultItemEnd = other.m_resultItemEnd;
	m_resultTitleStart = other.m_resultTitleStart;
	m_resultTitleEnd = other.m_resultTitleEnd;
	m_resultLinkStart = other.m_resultLinkStart;
	m_resultLinkEnd = other.m_resultLinkEnd;
	m_resultExtractStart = other.m_resultExtractStart;
	m_resultExtractEnd = other.m_resultExtractEnd;
	m_skipLocal = other.m_skipLocal;

	copy(other.m_languages.begin(), other.m_languages.end(),
		inserter(m_languages, m_languages.begin()));
	copy(other.m_outputEncodings.begin(), other.m_outputEncodings.end(),
		inserter(m_outputEncodings, m_outputEncodings.begin()));
	copy(other.m_inputEncodings.begin(), other.m_inputEncodings.end(),
		inserter(m_inputEncodings, m_inputEncodings.begin()));
	copy(other.m_parameters.begin(), other.m_parameters.end(),
		inserter(m_parameters, m_parameters.begin()));
}

bool SearchPluginProperties::operator==(const SearchPluginProperties &other) const
{
	if (m_name == other.m_name)
	{
		return true;
	}

	return false;
}

bool SearchPluginProperties::operator<(const SearchPluginProperties &other) const
{
	if (m_name < other.m_name)
	{
		return true;
	}

	return false;
}
