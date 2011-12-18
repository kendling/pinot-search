// generated 2007/10/14 16:21:28 SGT by fabrice@amra.dyndns.org.(none)
// using glademm V2.12.1
//
// DO NOT EDIT THIS FILE ! It was created using
// glade-- /home/fabrice/Projects/MetaSE/pinot/UI/GTK2/metase-gtk2.glade
// for gtk 2.10.14 and gtkmm 2.10.11
//
// Please modify the corresponding derived classes in ./src/propertiesDialog.cc


#if defined __GNUC__ && __GNUC__ < 3
#error This program will crash if compiled with g++ 2.x
// see the dynamic_cast bug in the gtkmm FAQ
#endif //
#include "config.h"
/*
 * Standard gettext macros.
 */
#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (GETTEXT_PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#endif
#include <gtkmmconfig.h>
#if GTKMM_MAJOR_VERSION==2 && GTKMM_MINOR_VERSION>2
#include <sigc++/sigc++.h>
#define GMM_GTKMM_22_24(a,b) b
#else //gtkmm 2.2
#define GMM_GTKMM_22_24(a,b) a
#endif //
#include "propertiesDialog_glade.hh"
#include <gdk/gdkkeysyms.h>
#include <gtkmm/accelgroup.h>
#include <gtkmm/button.h>
#include <gtkmm/buttonbox.h>
#include <gtkmm/box.h>
#include <gtkmm/table.h>
#include <gtkmm/viewport.h>
#include <gtkmm/adjustment.h>
#ifndef ENABLE_NLS
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif


propertiesDialog_glade::propertiesDialog_glade(
)
{  propertiesDialog = this;
   gmm_data = new GlademmData(get_accel_group());
   
   Gtk::Button *cancelbutton2 = Gtk::manage(new class Gtk::Button(Gtk::StockID("gtk-cancel")));
   labelOkButton = Gtk::manage(new class Gtk::Button(Gtk::StockID("gtk-ok")));
   titleLabel = Gtk::manage(new class Gtk::Label(_("Title:")));
   languageLabel = Gtk::manage(new class Gtk::Label(_("Language:")));
   typeLabel = Gtk::manage(new class Gtk::Label(_("MIME type:")));
   sizeLabel = Gtk::manage(new class Gtk::Label(_("Size:")));
   termsLabel = Gtk::manage(new class Gtk::Label(_("Terms:")));
   
   Gtk::VBox *propsLeftVbox = Gtk::manage(new class Gtk::VBox(false, 0));
   titleEntry = Gtk::manage(new class Gtk::Entry());
   typeEntry = Gtk::manage(new class Gtk::Entry());
   languageCombobox = Gtk::manage(new class Gtk::ComboBoxText());
   
   Gtk::Table *propertiesFirstTable = Gtk::manage(new class Gtk::Table(2, 2, false));
   sizeEntry = Gtk::manage(new class Gtk::Entry());
   termsEntry = Gtk::manage(new class Gtk::Entry());
   bytesLabel = Gtk::manage(new class Gtk::Label(_("bytes")));
   saveTermsButton = Gtk::manage(new class Gtk::Button(Gtk::StockID("gtk-save-as")));
   
   Gtk::Table *propertiesSecondTable = Gtk::manage(new class Gtk::Table(2, 2, false));
   Gtk::VBox *propsRightVbox = Gtk::manage(new class Gtk::VBox(false, 0));
   Gtk::HBox *propertiesHbox = Gtk::manage(new class Gtk::HBox(false, 0));
   labelsTreeview = Gtk::manage(new class Gtk::TreeView());
   
#if GTK_VERSION_LT(3, 0)
   Gtk::Viewport *viewport1 = Gtk::manage(new class Gtk::Viewport(*manage(new Gtk::Adjustment(0,0,1)), *manage(new Gtk::Adjustment(0,0,1))));
#else
   Gtk::Viewport *viewport1 = Gtk::manage(new class Gtk::Viewport(Gtk::Adjustment::create(0,0,1), Gtk::Adjustment::create(0,0,1)));
#endif
   labelsScrolledwindow = Gtk::manage(new class Gtk::ScrolledWindow());
   
   Gtk::VBox *propertiesVbox = Gtk::manage(new class Gtk::VBox(false, 0));
#if GTK_VERSION_LT(3, 0)
   cancelbutton2->set_flags(Gtk::CAN_FOCUS);
   cancelbutton2->set_flags(Gtk::CAN_DEFAULT);
#else
   cancelbutton2->set_can_focus();
   cancelbutton2->set_can_default();
#endif
   cancelbutton2->set_relief(Gtk::RELIEF_NORMAL);
#if GTK_VERSION_LT(3, 0)
   labelOkButton->set_flags(Gtk::CAN_FOCUS);
   labelOkButton->set_flags(Gtk::CAN_DEFAULT);
#else
   labelOkButton->set_can_focus();
   labelOkButton->set_can_default();
#endif
   labelOkButton->set_relief(Gtk::RELIEF_NORMAL);
   propertiesDialog->get_action_area()->property_layout_style().set_value(Gtk::BUTTONBOX_END);
   titleLabel->set_alignment(0,0.5);
   titleLabel->set_padding(0,0);
   titleLabel->set_justify(Gtk::JUSTIFY_LEFT);
   titleLabel->set_line_wrap(false);
   titleLabel->set_use_markup(false);
   titleLabel->set_selectable(false);
   titleLabel->set_ellipsize(Pango::ELLIPSIZE_NONE);
   titleLabel->set_width_chars(-1);
   titleLabel->set_angle(0);
   titleLabel->set_single_line_mode(false);
   languageLabel->set_alignment(0,0.5);
   languageLabel->set_padding(0,0);
   languageLabel->set_justify(Gtk::JUSTIFY_LEFT);
   languageLabel->set_line_wrap(false);
   languageLabel->set_use_markup(false);
   languageLabel->set_selectable(false);
   languageLabel->set_ellipsize(Pango::ELLIPSIZE_NONE);
   languageLabel->set_width_chars(-1);
   languageLabel->set_angle(0);
   languageLabel->set_single_line_mode(false);
   typeLabel->set_alignment(0,0.5);
   typeLabel->set_padding(0,0);
   typeLabel->set_justify(Gtk::JUSTIFY_LEFT);
   typeLabel->set_line_wrap(false);
   typeLabel->set_use_markup(false);
   typeLabel->set_selectable(false);
   typeLabel->set_ellipsize(Pango::ELLIPSIZE_NONE);
   typeLabel->set_width_chars(-1);
   typeLabel->set_angle(0);
   typeLabel->set_single_line_mode(false);
   sizeLabel->set_alignment(0,0.5);
   sizeLabel->set_padding(0,0);
   sizeLabel->set_justify(Gtk::JUSTIFY_LEFT);
   sizeLabel->set_line_wrap(false);
   sizeLabel->set_use_markup(false);
   sizeLabel->set_selectable(false);
   sizeLabel->set_ellipsize(Pango::ELLIPSIZE_NONE);
   sizeLabel->set_width_chars(-1);
   sizeLabel->set_angle(0);
   sizeLabel->set_single_line_mode(false);
   termsLabel->set_alignment(0,0.5);
   termsLabel->set_padding(0,0);
   termsLabel->set_justify(Gtk::JUSTIFY_LEFT);
   termsLabel->set_line_wrap(false);
   termsLabel->set_use_markup(false);
   termsLabel->set_selectable(false);
   termsLabel->set_ellipsize(Pango::ELLIPSIZE_NONE);
   termsLabel->set_width_chars(-1);
   termsLabel->set_angle(0);
   termsLabel->set_single_line_mode(false);
   propsLeftVbox->pack_start(*titleLabel, Gtk::PACK_EXPAND_WIDGET, 4);
   propsLeftVbox->pack_start(*languageLabel, Gtk::PACK_EXPAND_WIDGET, 4);
   propsLeftVbox->pack_start(*typeLabel, Gtk::PACK_EXPAND_WIDGET, 4);
   propsLeftVbox->pack_start(*sizeLabel, Gtk::PACK_EXPAND_WIDGET, 4);
   propsLeftVbox->pack_start(*termsLabel, Gtk::PACK_EXPAND_WIDGET, 4);
#if GTK_VERSION_LT(3, 0)
   titleEntry->set_flags(Gtk::CAN_FOCUS);
#else
   titleEntry->set_can_focus();
#endif
   titleEntry->set_visibility(true);
   titleEntry->set_editable(true);
   titleEntry->set_max_length(0);
   titleEntry->set_has_frame(true);
   titleEntry->set_activates_default(false);
#if GTK_VERSION_LT(3, 0)
   typeEntry->set_flags(Gtk::CAN_FOCUS);
#else
   typeEntry->set_can_focus();
#endif
   typeEntry->set_visibility(true);
   typeEntry->set_editable(false);
   typeEntry->set_max_length(0);
   typeEntry->set_has_frame(true);
   typeEntry->set_activates_default(false);
   propertiesFirstTable->set_row_spacings(0);
   propertiesFirstTable->set_col_spacings(0);
   propertiesFirstTable->attach(*titleEntry, 0, 1, 0, 1, Gtk::EXPAND|Gtk::FILL, Gtk::FILL, 4, 4);
   propertiesFirstTable->attach(*typeEntry, 0, 1, 2, 3, Gtk::EXPAND|Gtk::FILL, Gtk::FILL, 4, 4);
   propertiesFirstTable->attach(*languageCombobox, 0, 1, 1, 2, Gtk::EXPAND|Gtk::FILL, Gtk::FILL, 4, 4);
#if GTK_VERSION_LT(3, 0)
   sizeEntry->set_flags(Gtk::CAN_FOCUS);
#else
   sizeEntry->set_can_focus();
#endif
   sizeEntry->set_visibility(true);
   sizeEntry->set_editable(false);
   sizeEntry->set_max_length(0);
   sizeEntry->set_has_frame(true);
   sizeEntry->set_activates_default(false);
#if GTK_VERSION_LT(3, 0)
   termsEntry->set_flags(Gtk::CAN_FOCUS);
#else
   termsEntry->set_can_focus();
#endif
   termsEntry->set_visibility(true);
   termsEntry->set_editable(false);
   termsEntry->set_max_length(0);
   termsEntry->set_has_frame(true);
   termsEntry->set_activates_default(false);
   bytesLabel->set_alignment(0,0.5);
   bytesLabel->set_padding(0,0);
   bytesLabel->set_justify(Gtk::JUSTIFY_LEFT);
   bytesLabel->set_line_wrap(false);
   bytesLabel->set_use_markup(false);
   bytesLabel->set_selectable(false);
   bytesLabel->set_ellipsize(Pango::ELLIPSIZE_NONE);
   bytesLabel->set_width_chars(-1);
   bytesLabel->set_angle(0);
   bytesLabel->set_single_line_mode(false);
#if GTK_VERSION_LT(3, 0)
   saveTermsButton->set_flags(Gtk::CAN_FOCUS);
#else
   saveTermsButton->set_can_focus();
#endif
   saveTermsButton->set_relief(Gtk::RELIEF_NORMAL);
   propertiesSecondTable->set_row_spacings(0);
   propertiesSecondTable->set_col_spacings(0);
   propertiesSecondTable->attach(*sizeEntry, 0, 1, 0, 1, Gtk::EXPAND|Gtk::FILL, Gtk::FILL, 4, 4);
   propertiesSecondTable->attach(*termsEntry, 0, 1, 1, 2, Gtk::EXPAND|Gtk::FILL, Gtk::FILL, 4, 4);
   propertiesSecondTable->attach(*bytesLabel, 1, 2, 0, 1, Gtk::FILL, Gtk::FILL, 4, 4);
   propertiesSecondTable->attach(*saveTermsButton, 1, 2, 1, 2, Gtk::FILL, Gtk::FILL, 4, 4);
   propsRightVbox->pack_start(*propertiesFirstTable);
   propsRightVbox->pack_start(*propertiesSecondTable);
   propertiesHbox->pack_start(*propsLeftVbox);
   propertiesHbox->pack_start(*propsRightVbox);
#if GTK_VERSION_LT(3, 0)
   labelsTreeview->set_flags(Gtk::CAN_FOCUS);
#else
   labelsTreeview->set_can_focus();
#endif
   labelsTreeview->set_headers_visible(true);
   labelsTreeview->set_rules_hint(false);
   labelsTreeview->set_reorderable(false);
   labelsTreeview->set_enable_search(true);
#if GTKMM_MAJOR_VERSION==2 && GTKMM_MINOR_VERSION>=5
   labelsTreeview->set_fixed_height_mode(false);
   labelsTreeview->set_hover_selection(false);
   labelsTreeview->set_hover_expand(false);
#endif //
   viewport1->set_shadow_type(Gtk::SHADOW_IN);
   viewport1->add(*labelsTreeview);
#if GTK_VERSION_LT(3, 0)
   labelsScrolledwindow->set_flags(Gtk::CAN_FOCUS);
#else
   labelsScrolledwindow->set_can_focus();
#endif
   labelsScrolledwindow->set_shadow_type(Gtk::SHADOW_NONE);
   labelsScrolledwindow->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
   labelsScrolledwindow->property_window_placement().set_value(Gtk::CORNER_TOP_LEFT);
   labelsScrolledwindow->add(*viewport1);
   propertiesVbox->pack_start(*propertiesHbox, Gtk::PACK_SHRINK, 0);
   propertiesVbox->pack_start(*labelsScrolledwindow);
   propertiesDialog->get_vbox()->set_homogeneous(false);
   propertiesDialog->get_vbox()->set_spacing(0);
   propertiesDialog->get_vbox()->pack_start(*propertiesVbox);
   propertiesDialog->set_title(_("Properties"));
   propertiesDialog->set_modal(false);
   propertiesDialog->property_window_position().set_value(Gtk::WIN_POS_NONE);
   propertiesDialog->set_resizable(true);
   propertiesDialog->property_destroy_with_parent().set_value(false);
#if GTK_VERSION_LT(3, 0)
   propertiesDialog->set_has_separator(true);
#endif
   propertiesDialog->add_action_widget(*cancelbutton2, -6);
   propertiesDialog->add_action_widget(*labelOkButton, -5);
   cancelbutton2->show();
   labelOkButton->show();
   titleLabel->show();
   languageLabel->show();
   typeLabel->show();
   sizeLabel->show();
   termsLabel->show();
   propsLeftVbox->show();
   titleEntry->show();
   typeEntry->show();
   languageCombobox->show();
   propertiesFirstTable->show();
   sizeEntry->show();
   termsEntry->show();
   bytesLabel->show();
   saveTermsButton->show();
   propertiesSecondTable->show();
   propsRightVbox->show();
   propertiesHbox->show();
   labelsTreeview->show();
   viewport1->show();
   labelsScrolledwindow->show();
   propertiesVbox->show();
   labelOkButton->signal_clicked().connect(sigc::mem_fun(*this, &propertiesDialog_glade::on_labelOkButton_clicked), false);
   saveTermsButton->signal_clicked().connect(sigc::mem_fun(*this, &propertiesDialog_glade::on_saveTermsButton_clicked), false);
}

propertiesDialog_glade::~propertiesDialog_glade()
{  delete gmm_data;
}
