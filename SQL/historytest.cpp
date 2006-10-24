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

#include <iostream>

#include "QueryHistory.h"
#include "ViewHistory.h"

using namespace std;

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		cerr << "Usage: " << argv[0] << " <database> CREATE" << endl;
		return EXIT_FAILURE;
	}

	if (strncmp(argv[2], "CREATE", 6) == 0)
	{
		if ((QueryHistory::create(argv[1]) == true) &&
			(ViewHistory::create(argv[1]) == true))
		{
			cout << "Created database " << argv[1] << " and its tables" << endl;
		}
		else
		{
			cout << "Couldn't create database " << argv[1] << endl;
		}
	}

	return EXIT_SUCCESS;
}
