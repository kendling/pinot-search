// generated 2006/10/13 21:02:13 SGT by fabrice@amra.dyndns.org.(none)
// using glademm V2.12.1
//
// DO NOT EDIT THIS FILE ! It was created using
// glade-- /home/fabrice/Projects/MetaSE/pinot/UI/GTK2/metase-gtk2.glade
// for gtk 2.8.20 and gtkmm 2.8.8
//
// Please modify the corresponding derived classes in ./src/prefsDialog.cc


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
#include <sigc++/compatibility.h>
#include <sigc++/bind.h>
#define GMM_GTKMM_22_24(a,b) b
#else //gtkmm 2.2
#define GMM_GTKMM_22_24(a,b) a
#endif //
#include "prefsDialog_glade.hh"
#include <gdk/gdkkeysyms.h>
#include <gtkmm/accelgroup.h>
#include <gtkmm/buttonbox.h>
#include <gtkmm/label.h>
#include <gtkmm/table.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/image.h>
#include <gtkmm/box.h>
#include <gtkmm/alignment.h>
#ifndef ENABLE_NLS
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif


prefsDialog_glade::prefsDialog_glade(
)
{  prefsDialog = this;
   gmm_data = new GlademmData(get_accel_group());
   prefsCancelbutton = Gtk::manage(new class Gtk::Button(Gtk::StockID("gtk-cancel")));
   prefsOkbutton = Gtk::manage(new class Gtk::Button(Gtk::StockID("gtk-ok")));
   
   Gtk::Label *robotsLabel = Gtk::manage(new class Gtk::Label(_("HTTP crawling:")));
   apiKeyLabel = Gtk::manage(new class Gtk::Label(_("Google API key:")));
   apiKeyEntry = Gtk::manage(new class Gtk::Entry());
   ignoreRobotsCheckbutton = Gtk::manage(new class Gtk::CheckButton(_("Ignore robots.txt and Robots META tag")));
   
   Gtk::Label *newResultsLabel = Gtk::manage(new class Gtk::Label(_("New results:")));
   Gtk::Label *queriesLabel = Gtk::manage(new class Gtk::Label(_("Queries:")));
   enableCompletionCheckbutton = Gtk::manage(new class Gtk::CheckButton(_("Enable search terms suggestion")));
   newResultsColorbutton = Gtk::manage(new class Gtk::ColorButton());
   
   Gtk::Table *generalTable = Gtk::manage(new class Gtk::Table(2, 2, false));
   Gtk::Label *generalLabel = Gtk::manage(new class Gtk::Label(_("General")));
   Gtk::Label *indexLabelsLabel = Gtk::manage(new class Gtk::Label(_("Labels are used to classify indexed documents:")));
   labelsTreeview = Gtk::manage(new class Gtk::TreeView());
   
   Gtk::ScrolledWindow *labelsScrolledwindow = Gtk::manage(new class Gtk::ScrolledWindow());
   Gtk::Image *image235 = Gtk::manage(new class Gtk::Image(Gtk::StockID("gtk-add"), Gtk::IconSize(4)));
   Gtk::Label *label35 = Gtk::manage(new class Gtk::Label(_("Add")));
   Gtk::HBox *hbox29 = Gtk::manage(new class Gtk::HBox(false, 2));
   Gtk::Alignment *alignment17 = Gtk::manage(new class Gtk::Alignment(0.5, 0.5, 0, 0));
   addLabelButton = Gtk::manage(new class Gtk::Button());
   
   Gtk::Image *image236 = Gtk::manage(new class Gtk::Image(Gtk::StockID("gtk-remove"), Gtk::IconSize(4)));
   Gtk::Label *label36 = Gtk::manage(new class Gtk::Label(_("Remove")));
   Gtk::HBox *hbox30 = Gtk::manage(new class Gtk::HBox(false, 2));
   Gtk::Alignment *alignment18 = Gtk::manage(new class Gtk::Alignment(0.5, 0.5, 0, 0));
   removeLabelButton = Gtk::manage(new class Gtk::Button());
   
   Gtk::VButtonBox *labelsVbuttonbox = Gtk::manage(new class Gtk::VButtonBox(Gtk::BUTTONBOX_START, 0));
   Gtk::HBox *labelsHbox = Gtk::manage(new class Gtk::HBox(false, 0));
   Gtk::VBox *labelsVbox = Gtk::manage(new class Gtk::VBox(false, 0));
   Gtk::Label *labelsLabel = Gtk::manage(new class Gtk::Label(_("Labels")));
   Gtk::Label *directoriesLabel = Gtk::manage(new class Gtk::Label(_("These directories will be indexed and optionally monitored for changes:")));
   directoriesTreeview = Gtk::manage(new class Gtk::TreeView());
   
   Gtk::ScrolledWindow *directoriesScrolledwindow = Gtk::manage(new class Gtk::ScrolledWindow());
   Gtk::Image *image609 = Gtk::manage(new class Gtk::Image(Gtk::StockID("gtk-add"), Gtk::IconSize(4)));
   Gtk::Label *label58 = Gtk::manage(new class Gtk::Label(_("Add")));
   Gtk::HBox *hbox54 = Gtk::manage(new class Gtk::HBox(false, 2));
   Gtk::Alignment *alignment35 = Gtk::manage(new class Gtk::Alignment(0.5, 0.5, 0, 0));
   addDirectoryButton = Gtk::manage(new class Gtk::Button());
   removeDirectoryButton = Gtk::manage(new class Gtk::Button(Gtk::StockID("gtk-remove")));
   
   Gtk::VButtonBox *directoriesVbuttonbox = Gtk::manage(new class Gtk::VButtonBox(Gtk::BUTTONBOX_START, 0));
   Gtk::HBox *directoriesHbox = Gtk::manage(new class Gtk::HBox(false, 0));
   Gtk::Label *mailAccountsLabel = Gtk::manage(new class Gtk::Label(_("Mail boxes of type mbox can be monitored and indexed:")));
   mailTreeview = Gtk::manage(new class Gtk::TreeView());
   
   Gtk::ScrolledWindow *mailScrolledwindow = Gtk::manage(new class Gtk::ScrolledWindow());
   Gtk::Image *image497 = Gtk::manage(new class Gtk::Image(Gtk::StockID("gtk-add"), Gtk::IconSize(4)));
   Gtk::Label *label49 = Gtk::manage(new class Gtk::Label(_("Add")));
   Gtk::HBox *hbox42 = Gtk::manage(new class Gtk::HBox(false, 2));
   Gtk::Alignment *alignment28 = Gtk::manage(new class Gtk::Alignment(0.5, 0.5, 0, 0));
   addAccountButton = Gtk::manage(new class Gtk::Button());
   removeAccountButton = Gtk::manage(new class Gtk::Button(Gtk::StockID("gtk-remove")));
   
   Gtk::VButtonBox *mailVbuttonbox = Gtk::manage(new class Gtk::VButtonBox(Gtk::BUTTONBOX_START, 0));
   Gtk::HBox *mailHbox = Gtk::manage(new class Gtk::HBox(false, 0));
   Gtk::VBox *directoriesVbox = Gtk::manage(new class Gtk::VBox(false, 0));
   Gtk::Label *indexingLabel = Gtk::manage(new class Gtk::Label(_("Indexing")));
   prefsNotebook = Gtk::manage(new class Gtk::Notebook());
   prefsCancelbutton->set_flags(Gtk::CAN_FOCUS);
   prefsCancelbutton->set_flags(Gtk::CAN_DEFAULT);
   prefsCancelbutton->set_relief(Gtk::RELIEF_NORMAL);
   prefsOkbutton->set_flags(Gtk::CAN_FOCUS);
   prefsOkbutton->set_flags(Gtk::CAN_DEFAULT);
   prefsOkbutton->set_relief(Gtk::RELIEF_NORMAL);
   prefsDialog->get_action_area()->property_layout_style().set_value(Gtk::BUTTONBOX_END);
   robotsLabel->set_alignment(0,0.5);
   robotsLabel->set_padding(4,4);
   robotsLabel->set_justify(Gtk::JUSTIFY_LEFT);
   robotsLabel->set_line_wrap(false);
   robotsLabel->set_use_markup(false);
   robotsLabel->set_selectable(false);
   apiKeyLabel->set_alignment(0,0.5);
   apiKeyLabel->set_padding(4,4);
   apiKeyLabel->set_justify(Gtk::JUSTIFY_LEFT);
   apiKeyLabel->set_line_wrap(false);
   apiKeyLabel->set_use_markup(false);
   apiKeyLabel->set_selectable(false);
   apiKeyEntry->set_flags(Gtk::CAN_FOCUS);
   apiKeyEntry->set_visibility(true);
   apiKeyEntry->set_editable(true);
   apiKeyEntry->set_max_length(0);
   apiKeyEntry->set_has_frame(true);
   apiKeyEntry->set_activates_default(false);
   ignoreRobotsCheckbutton->set_flags(Gtk::CAN_FOCUS);
   ignoreRobotsCheckbutton->set_relief(Gtk::RELIEF_NORMAL);
   ignoreRobotsCheckbutton->set_mode(true);
   ignoreRobotsCheckbutton->set_active(false);
   newResultsLabel->set_alignment(0,0.5);
   newResultsLabel->set_padding(4,4);
   newResultsLabel->set_justify(Gtk::JUSTIFY_LEFT);
   newResultsLabel->set_line_wrap(false);
   newResultsLabel->set_use_markup(false);
   newResultsLabel->set_selectable(false);
   queriesLabel->set_alignment(0,0.5);
   queriesLabel->set_padding(4,4);
   queriesLabel->set_justify(Gtk::JUSTIFY_LEFT);
   queriesLabel->set_line_wrap(false);
   queriesLabel->set_use_markup(false);
   queriesLabel->set_selectable(false);
   enableCompletionCheckbutton->set_flags(Gtk::CAN_FOCUS);
   enableCompletionCheckbutton->set_relief(Gtk::RELIEF_NORMAL);
   enableCompletionCheckbutton->set_mode(true);
   enableCompletionCheckbutton->set_active(false);
   newResultsColorbutton->set_flags(Gtk::CAN_FOCUS);
   generalTable->set_row_spacings(0);
   generalTable->set_col_spacings(0);
   generalTable->attach(*robotsLabel, 0, 1, 0, 1, Gtk::FILL, Gtk::FILL, 0, 0);
   generalTable->attach(*apiKeyLabel, 0, 1, 3, 4, Gtk::FILL, Gtk::FILL, 0, 0);
   generalTable->attach(*apiKeyEntry, 1, 2, 3, 4, Gtk::EXPAND|Gtk::FILL, Gtk::FILL, 4, 4);
   generalTable->attach(*ignoreRobotsCheckbutton, 1, 2, 0, 1, Gtk::EXPAND|Gtk::FILL, Gtk::FILL, 4, 4);
   generalTable->attach(*newResultsLabel, 0, 1, 1, 2, Gtk::FILL, Gtk::FILL, 0, 0);
   generalTable->attach(*queriesLabel, 0, 1, 2, 3, Gtk::FILL, Gtk::FILL, 0, 0);
   generalTable->attach(*enableCompletionCheckbutton, 1, 2, 2, 3, Gtk::EXPAND|Gtk::FILL, Gtk::FILL, 4, 4);
   generalTable->attach(*newResultsColorbutton, 1, 2, 1, 2, Gtk::EXPAND|Gtk::FILL, Gtk::FILL, 4, 4);
   generalLabel->set_alignment(0.5,0.5);
   generalLabel->set_padding(0,0);
   generalLabel->set_justify(Gtk::JUSTIFY_LEFT);
   generalLabel->set_line_wrap(false);
   generalLabel->set_use_markup(false);
   generalLabel->set_selectable(false);
   indexLabelsLabel->set_alignment(0,0.5);
   indexLabelsLabel->set_padding(4,4);
   indexLabelsLabel->set_justify(Gtk::JUSTIFY_LEFT);
   indexLabelsLabel->set_line_wrap(true);
   indexLabelsLabel->set_use_markup(false);
   indexLabelsLabel->set_selectable(false);
   labelsTreeview->set_flags(Gtk::CAN_FOCUS);
   labelsTreeview->set_headers_visible(true);
   labelsTreeview->set_rules_hint(false);
   labelsTreeview->set_reorderable(false);
   labelsTreeview->set_enable_search(true);
   labelsScrolledwindow->set_flags(Gtk::CAN_FOCUS);
   labelsScrolledwindow->set_border_width(4);
   labelsScrolledwindow->set_shadow_type(Gtk::SHADOW_NONE);
   labelsScrolledwindow->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
   labelsScrolledwindow->property_window_placement().set_value(Gtk::CORNER_TOP_LEFT);
   labelsScrolledwindow->add(*labelsTreeview);
   image235->set_alignment(0.5,0.5);
   image235->set_padding(0,0);
   label35->set_alignment(0.5,0.5);
   label35->set_padding(0,0);
   label35->set_justify(Gtk::JUSTIFY_LEFT);
   label35->set_line_wrap(false);
   label35->set_use_markup(false);
   label35->set_selectable(false);
   hbox29->pack_start(*image235, Gtk::PACK_SHRINK, 0);
   hbox29->pack_start(*label35, Gtk::PACK_SHRINK, 0);
   alignment17->add(*hbox29);
   addLabelButton->set_flags(Gtk::CAN_FOCUS);
   addLabelButton->set_flags(Gtk::CAN_DEFAULT);
   addLabelButton->set_border_width(4);
   addLabelButton->set_relief(Gtk::RELIEF_NORMAL);
   addLabelButton->add(*alignment17);
   image236->set_alignment(0.5,0.5);
   image236->set_padding(0,0);
   label36->set_alignment(0.5,0.5);
   label36->set_padding(0,0);
   label36->set_justify(Gtk::JUSTIFY_LEFT);
   label36->set_line_wrap(false);
   label36->set_use_markup(false);
   label36->set_selectable(false);
   hbox30->pack_start(*image236, Gtk::PACK_SHRINK, 0);
   hbox30->pack_start(*label36, Gtk::PACK_SHRINK, 0);
   alignment18->add(*hbox30);
   removeLabelButton->set_flags(Gtk::CAN_FOCUS);
   removeLabelButton->set_flags(Gtk::CAN_DEFAULT);
   removeLabelButton->set_border_width(4);
   removeLabelButton->set_relief(Gtk::RELIEF_NORMAL);
   removeLabelButton->add(*alignment18);
   labelsVbuttonbox->pack_start(*addLabelButton);
   labelsVbuttonbox->pack_start(*removeLabelButton);
   labelsHbox->pack_start(*labelsScrolledwindow);
   labelsHbox->pack_start(*labelsVbuttonbox, Gtk::PACK_SHRINK, 0);
   labelsVbox->pack_start(*indexLabelsLabel, Gtk::PACK_SHRINK, 4);
   labelsVbox->pack_start(*labelsHbox, Gtk::PACK_EXPAND_WIDGET, 4);
   labelsLabel->set_alignment(0.5,0.5);
   labelsLabel->set_padding(0,0);
   labelsLabel->set_justify(Gtk::JUSTIFY_LEFT);
   labelsLabel->set_line_wrap(false);
   labelsLabel->set_use_markup(false);
   labelsLabel->set_selectable(false);
   directoriesLabel->set_alignment(0,0.5);
   directoriesLabel->set_padding(4,4);
   directoriesLabel->set_justify(Gtk::JUSTIFY_LEFT);
   directoriesLabel->set_line_wrap(true);
   directoriesLabel->set_use_markup(false);
   directoriesLabel->set_selectable(false);
   directoriesTreeview->set_flags(Gtk::CAN_FOCUS);
   directoriesTreeview->set_headers_visible(true);
   directoriesTreeview->set_rules_hint(false);
   directoriesTreeview->set_reorderable(false);
   directoriesTreeview->set_enable_search(true);
   directoriesScrolledwindow->set_flags(Gtk::CAN_FOCUS);
   directoriesScrolledwindow->set_border_width(4);
   directoriesScrolledwindow->set_shadow_type(Gtk::SHADOW_NONE);
   directoriesScrolledwindow->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
   directoriesScrolledwindow->property_window_placement().set_value(Gtk::CORNER_TOP_LEFT);
   directoriesScrolledwindow->add(*directoriesTreeview);
   image609->set_alignment(0.5,0.5);
   image609->set_padding(0,0);
   label58->set_alignment(0.5,0.5);
   label58->set_padding(0,0);
   label58->set_justify(Gtk::JUSTIFY_LEFT);
   label58->set_line_wrap(false);
   label58->set_use_markup(false);
   label58->set_selectable(false);
   hbox54->pack_start(*image609, Gtk::PACK_SHRINK, 0);
   hbox54->pack_start(*label58, Gtk::PACK_SHRINK, 0);
   alignment35->add(*hbox54);
   addDirectoryButton->set_flags(Gtk::CAN_FOCUS);
   addDirectoryButton->set_flags(Gtk::CAN_DEFAULT);
   addDirectoryButton->set_border_width(4);
   addDirectoryButton->set_relief(Gtk::RELIEF_NORMAL);
   addDirectoryButton->add(*alignment35);
   removeDirectoryButton->set_flags(Gtk::CAN_FOCUS);
   removeDirectoryButton->set_flags(Gtk::CAN_DEFAULT);
   removeDirectoryButton->set_border_width(4);
   removeDirectoryButton->set_relief(Gtk::RELIEF_NORMAL);
   directoriesVbuttonbox->pack_start(*addDirectoryButton);
   directoriesVbuttonbox->pack_start(*removeDirectoryButton);
   directoriesHbox->pack_start(*directoriesScrolledwindow);
   directoriesHbox->pack_start(*directoriesVbuttonbox, Gtk::PACK_SHRINK, 0);
   mailAccountsLabel->set_alignment(0,0.5);
   mailAccountsLabel->set_padding(4,4);
   mailAccountsLabel->set_justify(Gtk::JUSTIFY_LEFT);
   mailAccountsLabel->set_line_wrap(true);
   mailAccountsLabel->set_use_markup(false);
   mailAccountsLabel->set_selectable(false);
   mailTreeview->set_flags(Gtk::CAN_FOCUS);
   mailTreeview->set_headers_visible(true);
   mailTreeview->set_rules_hint(false);
   mailTreeview->set_reorderable(false);
   mailTreeview->set_enable_search(true);
   mailScrolledwindow->set_flags(Gtk::CAN_FOCUS);
   mailScrolledwindow->set_border_width(4);
   mailScrolledwindow->set_shadow_type(Gtk::SHADOW_NONE);
   mailScrolledwindow->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
   mailScrolledwindow->property_window_placement().set_value(Gtk::CORNER_TOP_LEFT);
   mailScrolledwindow->add(*mailTreeview);
   image497->set_alignment(0.5,0.5);
   image497->set_padding(0,0);
   label49->set_alignment(0.5,0.5);
   label49->set_padding(0,0);
   label49->set_justify(Gtk::JUSTIFY_LEFT);
   label49->set_line_wrap(false);
   label49->set_use_markup(false);
   label49->set_selectable(false);
   hbox42->pack_start(*image497, Gtk::PACK_SHRINK, 0);
   hbox42->pack_start(*label49, Gtk::PACK_SHRINK, 0);
   alignment28->add(*hbox42);
   addAccountButton->set_flags(Gtk::CAN_FOCUS);
   addAccountButton->set_flags(Gtk::CAN_DEFAULT);
   addAccountButton->set_border_width(4);
   addAccountButton->set_relief(Gtk::RELIEF_NORMAL);
   addAccountButton->add(*alignment28);
   removeAccountButton->set_flags(Gtk::CAN_FOCUS);
   removeAccountButton->set_flags(Gtk::CAN_DEFAULT);
   removeAccountButton->set_border_width(4);
   removeAccountButton->set_relief(Gtk::RELIEF_NORMAL);
   mailVbuttonbox->pack_start(*addAccountButton);
   mailVbuttonbox->pack_start(*removeAccountButton);
   mailHbox->pack_start(*mailScrolledwindow);
   mailHbox->pack_start(*mailVbuttonbox, Gtk::PACK_SHRINK, 0);
   directoriesVbox->pack_start(*directoriesLabel, Gtk::PACK_SHRINK, 4);
   directoriesVbox->pack_start(*directoriesHbox, Gtk::PACK_EXPAND_WIDGET, 4);
   directoriesVbox->pack_start(*mailAccountsLabel, Gtk::PACK_SHRINK, 4);
   directoriesVbox->pack_start(*mailHbox, Gtk::PACK_EXPAND_WIDGET, 4);
   indexingLabel->set_alignment(0.5,0.5);
   indexingLabel->set_padding(0,0);
   indexingLabel->set_justify(Gtk::JUSTIFY_LEFT);
   indexingLabel->set_line_wrap(false);
   indexingLabel->set_use_markup(false);
   indexingLabel->set_selectable(false);
   prefsNotebook->set_flags(Gtk::CAN_FOCUS);
   prefsNotebook->set_show_tabs(true);
   prefsNotebook->set_show_border(true);
   prefsNotebook->set_tab_pos(Gtk::POS_TOP);
   prefsNotebook->set_scrollable(false);
   prefsNotebook->append_page(*generalTable, *generalLabel);
   prefsNotebook->pages().back().set_tab_label_packing(false, true, Gtk::PACK_START);
   prefsNotebook->append_page(*labelsVbox, *labelsLabel);
   prefsNotebook->pages().back().set_tab_label_packing(false, true, Gtk::PACK_START);
   prefsNotebook->append_page(*directoriesVbox, *indexingLabel);
   prefsNotebook->pages().back().set_tab_label_packing(false, true, Gtk::PACK_START);
   prefsDialog->get_vbox()->set_homogeneous(false);
   prefsDialog->get_vbox()->set_spacing(0);
   prefsDialog->get_vbox()->pack_start(*prefsNotebook);
   prefsDialog->set_title(_("Preferences"));
   prefsDialog->set_modal(false);
   prefsDialog->property_window_position().set_value(Gtk::WIN_POS_NONE);
   prefsDialog->set_resizable(true);
   prefsDialog->property_destroy_with_parent().set_value(false);
   prefsDialog->set_has_separator(true);
   prefsDialog->add_action_widget(*prefsCancelbutton, -6);
   prefsDialog->add_action_widget(*prefsOkbutton, -5);
   prefsCancelbutton->show();
   prefsOkbutton->show();
   robotsLabel->show();
   apiKeyLabel->show();
   apiKeyEntry->show();
   ignoreRobotsCheckbutton->show();
   newResultsLabel->show();
   queriesLabel->show();
   enableCompletionCheckbutton->show();
   newResultsColorbutton->show();
   generalTable->show();
   generalLabel->show();
   indexLabelsLabel->show();
   labelsTreeview->show();
   labelsScrolledwindow->show();
   image235->show();
   label35->show();
   hbox29->show();
   alignment17->show();
   addLabelButton->show();
   image236->show();
   label36->show();
   hbox30->show();
   alignment18->show();
   removeLabelButton->show();
   labelsVbuttonbox->show();
   labelsHbox->show();
   labelsVbox->show();
   labelsLabel->show();
   directoriesLabel->show();
   directoriesTreeview->show();
   directoriesScrolledwindow->show();
   image609->show();
   label58->show();
   hbox54->show();
   alignment35->show();
   addDirectoryButton->show();
   removeDirectoryButton->show();
   directoriesVbuttonbox->show();
   directoriesHbox->show();
   mailAccountsLabel->show();
   mailTreeview->show();
   mailScrolledwindow->show();
   image497->show();
   label49->show();
   hbox42->show();
   alignment28->show();
   addAccountButton->show();
   removeAccountButton->show();
   mailVbuttonbox->show();
   mailHbox->show();
   directoriesVbox->show();
   indexingLabel->show();
   prefsNotebook->show();
   prefsDialog->show();
   prefsOkbutton->signal_clicked().connect(SigC::slot(*this, &prefsDialog_glade::on_prefsOkbutton_clicked), false);
   addLabelButton->signal_clicked().connect(SigC::slot(*this, &prefsDialog_glade::on_addLabelButton_clicked), false);
   removeLabelButton->signal_clicked().connect(SigC::slot(*this, &prefsDialog_glade::on_removeLabelButton_clicked), false);
   addDirectoryButton->signal_clicked().connect(SigC::slot(*this, &prefsDialog_glade::on_addDirectoryButton_clicked), false);
   removeDirectoryButton->signal_clicked().connect(SigC::slot(*this, &prefsDialog_glade::on_removeDirectoryButton_clicked), false);
   addAccountButton->signal_clicked().connect(SigC::slot(*this, &prefsDialog_glade::on_addAccountButton_clicked), false);
   removeAccountButton->signal_clicked().connect(SigC::slot(*this, &prefsDialog_glade::on_removeAccountButton_clicked), false);
}

prefsDialog_glade::~prefsDialog_glade()
{  delete gmm_data;
}
