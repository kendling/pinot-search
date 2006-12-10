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

#ifndef _RESULT_H
#define _RESULT_H

#include <string>

#include "DocumentInfo.h"

/// A document returned by a search engine.
class Result : public DocumentInfo
{
	public:
		Result(const std::string &location, const std::string &title,
			const std::string &extract, const std::string &language,
			float percentageScore = 0.0);
		Result(const Result &other);
		virtual ~Result();

		Result &operator=(const Result &other);

		bool operator<(const Result& other) const;

		/// Sets the result extract.
		void setExtract(const std::string &extract);

		/// Gets the result extract.
		std::string getExtract(void) const;

		/// Returns the result score.
		float getScore(void) const;

	protected:
		std::string m_extract;
		float m_score;

};

#endif // _RESULT_H
