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

#include "config.h"
#include "NLS.h"
#include "ModelColumns.h"

ComboModelColumns::ComboModelColumns()
{
	add(m_name);
}

ComboModelColumns::~ComboModelColumns()
{
}

EnginesModelColumns::EnginesModelColumns()
{
	add(m_name);
	add(m_engineName);
	add(m_option);
	add(m_type);
}

EnginesModelColumns::~EnginesModelColumns()
{
}

QueryModelColumns::QueryModelColumns()
{
	add(m_name);
	add(m_lastRun);
	add(m_summary);
	add(m_properties);
}

QueryModelColumns::~QueryModelColumns()
{
}

ResultsModelColumns::ResultsModelColumns()
{
	add(m_text);
	add(m_url);
	add(m_indexed);
	add(m_viewed);
	add(m_rankDiff);
	add(m_score);
	add(m_scoreText);
	add(m_engines);
	add(m_indexes);
	add(m_type);
}

ResultsModelColumns::~ResultsModelColumns()
{
}

IndexModelColumns::IndexModelColumns()
{
	add(m_text);
	add(m_url);
	add(m_liveUrl);
	add(m_type);
	add(m_language);
	add(m_id);
	add(m_timestamp);
}

IndexModelColumns::~IndexModelColumns()
{
}

OtherIndexModelColumns::OtherIndexModelColumns()
{
	add(m_name);
	add(m_location);
}

OtherIndexModelColumns::~OtherIndexModelColumns()
{
}

LabelModelColumns::LabelModelColumns()
{
	add(m_enabled);
	add(m_name);
	add(m_oldName);
}

LabelModelColumns::~LabelModelColumns()
{
}

TimestampedModelColumns::TimestampedModelColumns()
{
	add(m_location);
	add(m_mTime);
}

TimestampedModelColumns::~TimestampedModelColumns()
{
}

IndexableModelColumns::IndexableModelColumns()
{
	add(m_monitor);
	add(m_location);
}

IndexableModelColumns::~IndexableModelColumns()
{
}
