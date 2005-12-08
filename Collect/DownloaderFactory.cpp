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

#include "XapianCollector.h"
#include "NeonDownloader.h"
#include "FileCollector.h"
#include "MboxCollector.h"
#include "DownloaderFactory.h"

/// Returns a Downloader of the specified type; NULL if unavailable.
DownloaderInterface *DownloaderFactory::getDownloader(string protocol, string type)
{
	DownloaderInterface *myDownloader = NULL;

	// Choice by protocol
	if (protocol == "http")
	{
		myDownloader = new NeonDownloader();
	}
	else if (protocol == "xapian")
	{
		myDownloader = new XapianCollector();
	}
	else if (protocol == "file")
	{
		myDownloader = new FileCollector();
	}
	else if (protocol == "mailbox")
	{
		myDownloader = new MboxCollector();
	}

	return myDownloader;
}
