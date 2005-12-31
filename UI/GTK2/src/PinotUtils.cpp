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

#include <iostream>
#include <gtkmm/stock.h>
#include <gtkmm/filechooserdialog.h>

#include "config.h"
#include "NLS.h"
#include "PinotUtils.h"

using namespace std;
using namespace SigC;
using namespace Glib;
using namespace Gtk;

bool select_file_name(Window &parentWindow, const ustring &title,
	ustring &location, bool openOrCreate, bool directoriesOnly)
{
	FileChooserAction chooserAction = FILE_CHOOSER_ACTION_OPEN;
	StockID okButtonStockId = Stock::OPEN;

	if (title.empty() == true)
	{
		return false;
	}

	// Have we been provided with an initial location ?
	if (location.empty() == true)
	{
		// No, get the location of the home directory then
		char *homeDir = getenv("HOME");
		if (homeDir != NULL)
		{
			location = homeDir;
			location += "/";
		}
	}

	if (directoriesOnly == false)
	{
		if (openOrCreate == true)
		{
			chooserAction = FILE_CHOOSER_ACTION_OPEN;
		}
		else
		{
			chooserAction = FILE_CHOOSER_ACTION_SAVE;
			okButtonStockId = Stock::SAVE;
		}
	}
	else
	{
		if (openOrCreate == true)
		{
			chooserAction = FILE_CHOOSER_ACTION_SELECT_FOLDER;
		}
		else
		{
			chooserAction = FILE_CHOOSER_ACTION_CREATE_FOLDER;
			okButtonStockId = Stock::SAVE;
		}
	}

	FileChooserDialog fileChooser(title, chooserAction);
	fileChooser.set_filename(filename_from_utf8(location));
	fileChooser.set_local_only();
	fileChooser.set_select_multiple(false);
	fileChooser.set_transient_for(parentWindow);
	// Add response buttons
	fileChooser.add_button(Stock::CANCEL, RESPONSE_CANCEL);
	fileChooser.add_button(okButtonStockId, RESPONSE_OK);
	// FIXME: add FileFilter's
	fileChooser.show();
	int result = fileChooser.run();
	if (result == RESPONSE_OK)
	{
		// Retrieve the chosen location
		if (directoriesOnly == false)
		{
			location = filename_to_utf8(fileChooser.get_filename());
		}
		else
		{
			location = filename_to_utf8(fileChooser.get_current_folder());
		}

		return true;
	}

	return false;
}

/// Create a resizable text column.
TreeViewColumn *create_resizable_column(const ustring &title, const TreeModelColumnBase& modelColumn)
{
	TreeViewColumn *treeColumn = new TreeViewColumn(title);

	CellRendererText *textCellRenderer = new CellRendererText();
	treeColumn->pack_start(*manage(textCellRenderer));
	treeColumn->add_attribute(textCellRenderer->property_text(), modelColumn);
	treeColumn->set_resizable(true);

	return treeColumn;
}

/// Create a resizable icon and text column, rendered by renderTextAndIconCell.
TreeViewColumn *create_resizable_column_with_icon(const ustring &title,
	const TreeModelColumnBase& modelColumn, const TreeViewColumn::SlotCellData &renderTextAndIconCell)
{
	TreeViewColumn *treeColumn = new TreeViewColumn(title);

	// Pack an icon renderer in the column
	CellRendererPixbuf *iconRenderer = new CellRendererPixbuf();
	treeColumn->pack_start(*manage(iconRenderer), false);
	treeColumn->set_cell_data_func(*iconRenderer, renderTextAndIconCell);
	// ...followed by a text renderer
	CellRendererText *textCellRenderer = new CellRendererText();
	treeColumn->pack_start(*manage(textCellRenderer));
	treeColumn->set_cell_data_func(*textCellRenderer, renderTextAndIconCell);

	treeColumn->add_attribute(textCellRenderer->property_text(), modelColumn);
	treeColumn->set_resizable(true);

	return treeColumn;
}

/// Converts to UTF-8.
ustring to_utf8(const string &text)
{
	std::string charset;

	// Get the locale charset
	get_charset(charset);
	// Call overload
	return to_utf8(text, charset);
}

/// Converts from the given charset to UTF-8.
ustring to_utf8(const string &text, const string &charset)
{
	if ((charset == "UTF-8") ||
		(charset == "utf-8"))
	{
		// No conversion necessary
		return text;
	}

	try
	{
		if (charset.empty() == false)
		{
			return convert_with_fallback(text, "UTF-8", charset);
		}
		else
		{
			return locale_to_utf8(text);
		}
	}
	catch (ConvertError &ce)
	{
	}

	return "";
}
