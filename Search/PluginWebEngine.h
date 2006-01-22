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

#ifndef _XML_WEB_ENGINE_H
#define _XML_WEB_ENGINE_H

#include <string>
#include <set>
#include <map>

#include "WebEngine.h"

using namespace std;

/**
  * A class that implements the Sherlock search plugin standard.
  * See http://developer.apple.com/technotes/tn/tn1141.html
  * and http://mycroft.mozdev.org/deepdocs/deepdocs.html
  */
class PluginWebEngine : public WebEngine
{
	public:
		PluginWebEngine(const string &fileName);
		virtual ~PluginWebEngine();

		/// Utility method that returns a search plugin's name and channel.
		static bool getDetails(const string &fileName, string &name, string &channel);

		/// Runs a query; true if success.
		virtual bool runQuery(QueryProperties& queryProps);

	protected:
		string m_name;
		string m_baseUrl;
		string m_channel;
		map<string, string> m_inputTags;
		string m_userInputTag;
		string m_resultListStart;
		string m_resultListEnd;
		string m_resultItemStart;
		string m_resultItemEnd;
		string m_resultTitleStart;
		string m_resultTitleEnd;
		string m_resultLinkStart;
		string m_resultLinkEnd;
		string m_resultExtractStart;
		string m_resultExtractEnd;
		bool m_skipLocal;
		string m_nextTag;
		unsigned int m_nextFactor;
		unsigned int m_nextBase;

		bool load(const string &fileName);

		bool getPage(const string &formattedQuery);

	private:
		PluginWebEngine(const PluginWebEngine &other);
		PluginWebEngine &operator=(const PluginWebEngine &other);

};

#endif // _XML_WEB_ENGINE_H
