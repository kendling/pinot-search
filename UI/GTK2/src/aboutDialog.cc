// generated 2003/5/18 11:20:09 BST by fabrice@amra.dyndns.org.(none)
// using glademm V2.0.0
//
// newer (non customized) versions of this file go to aboutDialog.cc_new

// This file is for your program, I won't touch it again!

#include <glibmm/ustring.h>

#include "config.h"
#include "NLS.h"
#include "aboutDialog.hh"

using namespace Glib;

aboutDialog::aboutDialog()
{
#ifdef VERSION
	ustring name = nameLabel->get_text();
	name += " v";
	name += VERSION;
	nameLabel->set_text(name);
#endif
}

aboutDialog::~aboutDialog()
{
}
