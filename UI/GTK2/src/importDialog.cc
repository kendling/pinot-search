// generated 2004/3/6 14:15:21 GMT by fabrice@amra.dyndns.org.(none)
// using glademm V2.0.0
//
// newer (non customized) versions of this file go to importDialog.cc_new

// This file is for your program, I won't touch it again!

#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <glibmm/convert.h>

#include "config.h"
#include "MIMEScanner.h"
#include "NLS.h"
#include "Url.h"
#include "PinotSettings.h"
#include "PinotUtils.h"
#include "importDialog.hh"

using namespace std;
using namespace SigC;
using namespace Glib;
using namespace Gtk;

string importDialog::m_directory = "";

importDialog::importDialog(const Glib::ustring &title,
	bool selectTitle, bool selectDepth, bool allowLocalOnly) :
	importDialog_glade(),
	m_docsCount(0),
	m_importDirectory(false),
	m_maxDirLevel(1)
{
	// Associate the columns model to the type combo
	m_refTypeList = ListStore::create(m_typeColumns);
	typeCombobox->set_model(m_refTypeList);
	typeCombobox->pack_start(m_typeColumns.m_name);
	// Populate
	populate_combobox(allowLocalOnly);

	// Initialize the default directory
	if (m_directory.empty() == true)
	{
		char *homeDir = getenv("HOME");
		if (homeDir != NULL)
		{
			m_directory = homeDir + string("/");
		}
	}

	set_title(title);

	if (selectTitle == false)
	{
		titleLabel->hide();
		titleEntry->hide();
	}

	if (selectDepth == false)
	{
		depthLabel->hide();
		depthSpinbutton->hide();
	}
	else
	{
		// The default type is not directory
		// FIXME: this could also apply to URLs !
		depthSpinbutton->set_sensitive(false);
		depthSpinbutton->set_value(m_maxDirLevel);
	}

	// Disable this button as long the location entry field is empty
	// The title field may remain empty
	importButton->set_sensitive(false);
}

importDialog::~importDialog()
{
}

void importDialog::populate_combobox(bool allowLocalOnly)
{
	bool foundLanguage = false;

	TreeModel::iterator iter = m_refTypeList->append();
	TreeModel::Row row = *iter;
	row[m_typeColumns.m_name] = _("Single file");
	iter = m_refTypeList->append();
	row = *iter;
	row[m_typeColumns.m_name] = _("Whole directory");
	if (allowLocalOnly == false)
	{
		iter = m_refTypeList->append();
		row = *iter;
		row[m_typeColumns.m_name] = _("URL");
	}

	typeCombobox->set_active(0);
}

void importDialog::scan_file(const string &fileName, unsigned int &level)
{
	struct stat fileStat;
	Url urlObj("file://" + fileName);

	if ((fileName.empty() == true) ||
		(urlObj.getFile() == ".") ||
		(urlObj.getFile() == ".."))
	{
		return;
	}

#ifdef DEBUG
	cout << "importDialog::scan_file: " << fileName << endl;
#endif
	if (lstat(fileName.c_str(), &fileStat) == -1)
	{
#ifdef DEBUG
		cout << "importDialog::scan_file: stat failed" << endl;
#endif
		return;
	}

	// Is it a file or a directory ?
	if (S_ISLNK(fileStat.st_mode))
	{
		cout << "importDialog::scan_file: skipping symlink" << endl;
		return;
	}
	else if (S_ISDIR(fileStat.st_mode))
	{
		// A directory : scan it
		DIR *pDir = opendir(fileName.c_str());
		if (pDir == NULL)
		{
			return;
		}

		// Iterate through this directory's entries
		struct dirent *pDirEntry = readdir(pDir);
		while (pDirEntry != NULL)
		{
			char *pEntryName = pDirEntry->d_name;
			if (pEntryName != NULL)
			{
				string location = fileName;
				if (fileName[fileName.length() - 1] != '/')
				{
					location += "/";
				}
				location += pEntryName;

				// Scan this entry
				if ((m_maxDirLevel == 0) ||
					(level < m_maxDirLevel))
				{
					++level;
					scan_file(location, level);
					--level;
				}
#ifdef DEBUG
				else cout << "importDialog::scan_file: not going deeper than level " << level << endl;
#endif
			}

			// Next entry
			pDirEntry = readdir(pDir);
		}
		closedir(pDir);
	}
	else if (S_ISREG(fileStat.st_mode))
	{
		// Build a valid URL
		string location = "file://";
		location += fileName;
		string title = locale_from_utf8(m_title);

		if (m_importDirectory == true)
		{
			title += " ";
			title += urlObj.getFile();
		}

		DocumentInfo docInfo(title, location,
			MIMEScanner::scanFile(fileName), "");

		import_file(urlObj.getFile(), docInfo);
	}
}

void importDialog::import_file(const string &fileName,
	const DocumentInfo &docInfo)
{
	// Fire up the signal
	m_signalImportFile(docInfo);
	++m_docsCount;
}

ustring importDialog::getDocumentTitle(void)
{
	return m_title;
}

unsigned int importDialog::getDocumentsCount(void)
{
	return m_docsCount;
}

Signal1<void, DocumentInfo> &importDialog::getImportFileSignal(void)
{
	return m_signalImportFile;
}

void importDialog::on_importButton_clicked()
{
	string location = locale_from_utf8(locationEntry->get_text());
	unsigned int level = 0;

	importButton->set_sensitive(false);

	// Title
	m_title = titleEntry->get_text();
	// Type
	if (typeCombobox->get_active_row_number() <= 1)
	{
		string::size_type pos = location.find_last_of("/");
		if (pos != string::npos)
		{
			// Update m_directory
			m_directory = location.substr(0, pos + 1);
#ifdef DEBUG
			cout << "importDialog::on_importButton_clicked: directory now " << m_directory << endl;
#endif
		}

		// Maximum depth
		m_maxDirLevel = (unsigned int)depthSpinbutton->get_value();

		scan_file(location, level);
	}
	else
	{
		Url urlObj(location);
		DocumentInfo docInfo(locale_from_utf8(m_title), location,
			MIMEScanner::scanUrl(urlObj), "");

		import_file(urlObj.getFile(), docInfo);
	}
}

void importDialog::on_selectButton_clicked()
{
	ustring fileName = to_utf8(m_directory);

	if (select_file_name(*this, _("Document To Import"), fileName, true, m_importDirectory) == true)
	{
		// Update the location
#ifdef DEBUG
		cout << "importDialog::on_selectButton_clicked: location is " << fileName << endl;
#endif
		locationEntry->set_text(fileName);
		ustring::size_type pos = fileName.find_last_of("/");
		if (pos != string::npos)
		{
			// Update m_directory
			m_directory = locale_from_utf8(fileName.substr(0, pos + 1));
#ifdef DEBUG
			cout << "importDialog::on_selectButton_clicked: directory now " << m_directory << endl;
#endif
		}
	}
}

void importDialog::on_locationEntry_changed()
{
	ustring fileName = locationEntry->get_text();
	bool enableOk = true;

	if (fileName.empty() == false)
	{
		unsigned int type = typeCombobox->get_active_row_number();

		// Check the entry makes sense
		if (type <= 1)
		{
			if (fileName[0] != '/')
			{
				enableOk = false;
			}
		}
		else
		{
			Url urlObj(locale_from_utf8(fileName));

			// Check the URL is valid
			if (urlObj.getProtocol().empty() == true)
			{
				enableOk = false;
			}
			// FIXME: be more thorough
		}
	}
	else
	{
		enableOk = false;
	}

	importButton->set_sensitive(enableOk);
}

void importDialog::on_typeCombobox_changed()
{
	unsigned int type = typeCombobox->get_active_row_number();
	bool selectLocation = true;

	m_importDirectory = false;
	if (type == 1)
	{
		m_importDirectory = true;
	}
	else if (type > 1)
	{
		// Disable the select button only if type is URL
		selectLocation = false;
	}

	// FIXME: this could also apply to URLs !
	depthSpinbutton->set_sensitive(m_importDirectory);
	selectButton->set_sensitive(selectLocation);
}
