// generated 2008/12/27 19:39:02 SGT by fabrice@rexor.dyndns.org.(none)
// using glademm V2.12.1
//
// DO NOT EDIT THIS FILE ! It was created using
// glade-- /home/fabrice/Projects/MetaSE/pinot/UI/GTK2/metase-gtk2.glade
// for gtk 2.14.5 and gtkmm 2.14.3
//
// Please modify the corresponding derived classes in ./src/prefsWindow.cc


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
#include "prefsWindow_glade.hh"
#include <gdk/gdkkeysyms.h>
#include <gtkmm/accelgroup.h>
#include <gtkmm/buttonbox.h>
#include <gtkmm/label.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/image.h>
#include <gtkmm/box.h>
#include <gtkmm/alignment.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/table.h>
#ifndef ENABLE_NLS
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif


prefsWindow_glade::prefsWindow_glade(
) : Gtk::Window(Gtk::WINDOW_TOPLEVEL)
{  
   
   Gtk::Window *prefsWindow = this;
   gmm_data = new GlademmData(get_accel_group());
   prefsCancelbutton = Gtk::manage(new class Gtk::Button(Gtk::StockID("gtk-cancel")));
   prefsOkbutton = Gtk::manage(new class Gtk::Button(Gtk::StockID("gtk-ok")));
   
   Gtk::HButtonBox *hbuttonbox1 = Gtk::manage(new class Gtk::HButtonBox(Gtk::BUTTONBOX_END, 0));
   Gtk::Label *directoriesLabel = Gtk::manage(new class Gtk::Label(_("These directories will be indexed and optionally monitored for changes:")));
   directoriesTreeview = Gtk::manage(new class Gtk::TreeView());
   
   Gtk::ScrolledWindow *scrolledwindow2 = Gtk::manage(new class Gtk::ScrolledWindow());
   Gtk::Image *image727 = Gtk::manage(new class Gtk::Image(Gtk::StockID("gtk-add"), Gtk::IconSize(4)));
   Gtk::Label *label75 = Gtk::manage(new class Gtk::Label(_("Add")));
   Gtk::HBox *hbox61 = Gtk::manage(new class Gtk::HBox(false, 2));
   Gtk::Alignment *alignment39 = Gtk::manage(new class Gtk::Alignment(0.5, 0.5, 0, 0));
   addDirectoryButton = Gtk::manage(new class Gtk::Button());
   removeDirectoryButton = Gtk::manage(new class Gtk::Button(Gtk::StockID("gtk-remove")));
   
   Gtk::VButtonBox *vbuttonbox2 = Gtk::manage(new class Gtk::VButtonBox(Gtk::BUTTONBOX_START, 0));
   Gtk::HBox *hbox60 = Gtk::manage(new class Gtk::HBox(false, 0));
   Gtk::Label *label76 = Gtk::manage(new class Gtk::Label(_("File patterns:")));
   patternsTreeview = Gtk::manage(new class Gtk::TreeView());
   
   Gtk::ScrolledWindow *scrolledwindow3 = Gtk::manage(new class Gtk::ScrolledWindow());
   patternsCombobox = Gtk::manage(new class Gtk::ComboBoxText());
   
   Gtk::VBox *vbox4 = Gtk::manage(new class Gtk::VBox(false, 0));
   Gtk::Image *image728 = Gtk::manage(new class Gtk::Image(Gtk::StockID("gtk-add"), Gtk::IconSize(4)));
   Gtk::Label *label77 = Gtk::manage(new class Gtk::Label(_("Add")));
   Gtk::HBox *hbox63 = Gtk::manage(new class Gtk::HBox(false, 2));
   Gtk::Alignment *alignment40 = Gtk::manage(new class Gtk::Alignment(0.5, 0.5, 0, 0));
   addPatternButton = Gtk::manage(new class Gtk::Button());
   removePatternButton = Gtk::manage(new class Gtk::Button(Gtk::StockID("gtk-remove")));
   resetPatternsButton = Gtk::manage(new class Gtk::Button(_("Reset")));
   
   Gtk::VButtonBox *vbuttonbox3 = Gtk::manage(new class Gtk::VButtonBox(Gtk::BUTTONBOX_START, 0));
   Gtk::HBox *hbox62 = Gtk::manage(new class Gtk::HBox(false, 0));
   Gtk::VBox *vbox3 = Gtk::manage(new class Gtk::VBox(false, 0));
   Gtk::Label *indexingLabel = Gtk::manage(new class Gtk::Label(_("Indexing")));
   Gtk::Label *indexLabelsLabel = Gtk::manage(new class Gtk::Label(_("Labels are used to classify indexed documents:")));
   labelsTreeview = Gtk::manage(new class Gtk::TreeView());
   
   Gtk::ScrolledWindow *scrolledwindow1 = Gtk::manage(new class Gtk::ScrolledWindow());
   Gtk::Image *image725 = Gtk::manage(new class Gtk::Image(Gtk::StockID("gtk-add"), Gtk::IconSize(4)));
   Gtk::Label *label71 = Gtk::manage(new class Gtk::Label(_("Add")));
   Gtk::HBox *hbox58 = Gtk::manage(new class Gtk::HBox(false, 2));
   Gtk::Alignment *alignment37 = Gtk::manage(new class Gtk::Alignment(0.5, 0.5, 0, 0));
   addLabelButton = Gtk::manage(new class Gtk::Button());
   
   Gtk::Image *image726 = Gtk::manage(new class Gtk::Image(Gtk::StockID("gtk-remove"), Gtk::IconSize(4)));
   Gtk::Label *label72 = Gtk::manage(new class Gtk::Label(_("Remove")));
   Gtk::HBox *hbox59 = Gtk::manage(new class Gtk::HBox(false, 2));
   Gtk::Alignment *alignment38 = Gtk::manage(new class Gtk::Alignment(0.5, 0.5, 0, 0));
   removeLabelButton = Gtk::manage(new class Gtk::Button());
   
   Gtk::VButtonBox *vbuttonbox1 = Gtk::manage(new class Gtk::VButtonBox(Gtk::BUTTONBOX_START, 0));
   Gtk::HBox *hbox57 = Gtk::manage(new class Gtk::HBox(false, 0));
   Gtk::VBox *vbox2 = Gtk::manage(new class Gtk::VBox(false, 0));
   Gtk::Label *labelsLabel = Gtk::manage(new class Gtk::Label(_("Labels")));
   Gtk::RadioButton::Group _RadioBGroup_directConnectionRadiobutton;
   directConnectionRadiobutton = Gtk::manage(new class Gtk::RadioButton(_RadioBGroup_directConnectionRadiobutton, _("Direct connection to the Internet")));
   proxyRadiobutton = Gtk::manage(new class Gtk::RadioButton(_RadioBGroup_directConnectionRadiobutton, _("Manual proxy configuration:")));
   
   Gtk::Label *proxyAddressLabel = Gtk::manage(new class Gtk::Label(_("Address:")));
   proxyAddressEntry = Gtk::manage(new class Gtk::Entry());
   
   Gtk::Label *proxyTypeLabel = Gtk::manage(new class Gtk::Label(_("Type:")));
   Gtk::Label *proxyPortLabel = Gtk::manage(new class Gtk::Label(_("Port:")));
#if GTK_VERSION_LT(3, 0)
   Gtk::Adjustment *proxyPortSpinbutton_adj = Gtk::manage(new class Gtk::Adjustment(80, 1, 65535, 1, 10, 0));
   proxyPortSpinbutton = Gtk::manage(new class Gtk::SpinButton(*proxyPortSpinbutton_adj, 1, 0));
#else
   Glib::RefPtr<Gtk::Adjustment> proxyPortSpinbutton_adj = Gtk::Adjustment::create(80, 1, 65535, 1, 10, 0);
   proxyPortSpinbutton = Gtk::manage(new class Gtk::SpinButton(proxyPortSpinbutton_adj, 1, 0));
#endif
   proxyTypeCombobox = Gtk::manage(new class Gtk::ComboBoxText());
   
   Gtk::Table *table3 = Gtk::manage(new class Gtk::Table(2, 2, false));
   Gtk::Table *table2 = Gtk::manage(new class Gtk::Table(2, 2, false));
   Gtk::Label *networkLabel = Gtk::manage(new class Gtk::Label(_("Network")));
   apiKeyLabel = Gtk::manage(new class Gtk::Label(_("Google API key:")));
   apiKeyEntry = Gtk::manage(new class Gtk::Entry());
   
   Gtk::Label *queriesLabel = Gtk::manage(new class Gtk::Label(_("Queries:")));
   enableCompletionCheckbutton = Gtk::manage(new class Gtk::CheckButton(_("Enable search terms suggestion")));
   newResultsColorbutton = Gtk::manage(new class Gtk::ColorButton());
   ignoreRobotsCheckbutton = Gtk::manage(new class Gtk::CheckButton(_("Ignore robots.txt and Robots META tag")));
   robotsLabels = Gtk::manage(new class Gtk::Label(_("HTTP crawling:")));
   
   Gtk::Label *newResultsLabel = Gtk::manage(new class Gtk::Label(_("New results:")));
   generalTable = Gtk::manage(new class Gtk::Table(2, 2, false));
   
   Gtk::Label *miscLabel = Gtk::manage(new class Gtk::Label(_("Miscellaneous")));
   prefsNotebook = Gtk::manage(new class Gtk::Notebook());
   
   Gtk::VBox *vbox1 = Gtk::manage(new class Gtk::VBox(false, 0));
#if GTK_VERSION_LT(3, 0)
   prefsCancelbutton->set_flags(Gtk::CAN_FOCUS);
   prefsCancelbutton->set_flags(Gtk::CAN_DEFAULT);
#else
   prefsCancelbutton->set_can_focus();
   prefsCancelbutton->set_can_default();
#endif
   prefsCancelbutton->set_border_width(4);
   prefsCancelbutton->set_relief(Gtk::RELIEF_NORMAL);
#if GTK_VERSION_LT(3, 0)
   prefsOkbutton->set_can_focus();
   prefsOkbutton->set_can_default();
#else
#endif
   prefsOkbutton->set_border_width(4);
   prefsOkbutton->set_relief(Gtk::RELIEF_NORMAL);
   hbuttonbox1->property_layout_style().set_value(Gtk::BUTTONBOX_END);
   hbuttonbox1->pack_start(*prefsCancelbutton);
   hbuttonbox1->pack_start(*prefsOkbutton);
   directoriesLabel->set_alignment(0,0.5);
   directoriesLabel->set_padding(4,4);
   directoriesLabel->set_justify(Gtk::JUSTIFY_LEFT);
   directoriesLabel->set_line_wrap(true);
   directoriesLabel->set_use_markup(false);
   directoriesLabel->set_selectable(false);
   directoriesLabel->set_ellipsize(Pango::ELLIPSIZE_NONE);
   directoriesLabel->set_width_chars(-1);
   directoriesLabel->set_angle(0);
   directoriesLabel->set_single_line_mode(false);
#if GTK_VERSION_LT(3, 0)
   directoriesTreeview->set_flags(Gtk::CAN_FOCUS);
#else
   directoriesTreeview->set_can_focus();
#endif
   directoriesTreeview->set_headers_visible(true);
   directoriesTreeview->set_rules_hint(false);
   directoriesTreeview->set_reorderable(false);
   directoriesTreeview->set_enable_search(true);
#if GTK_VERSION_LT(3, 0)
   scrolledwindow2->set_flags(Gtk::CAN_FOCUS);
#else
   scrolledwindow2->set_can_focus();
#endif
   scrolledwindow2->set_border_width(4);
   scrolledwindow2->set_shadow_type(Gtk::SHADOW_NONE);
   scrolledwindow2->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
   scrolledwindow2->property_window_placement().set_value(Gtk::CORNER_TOP_LEFT);
   scrolledwindow2->add(*directoriesTreeview);
   image727->set_alignment(0.5,0.5);
   image727->set_padding(0,0);
   label75->set_alignment(0.5,0.5);
   label75->set_padding(0,0);
   label75->set_justify(Gtk::JUSTIFY_LEFT);
   label75->set_line_wrap(false);
   label75->set_use_markup(false);
   label75->set_selectable(false);
   label75->set_ellipsize(Pango::ELLIPSIZE_NONE);
   label75->set_width_chars(-1);
   label75->set_angle(0);
   label75->set_single_line_mode(false);
   hbox61->pack_start(*image727, Gtk::PACK_SHRINK, 0);
   hbox61->pack_start(*label75, Gtk::PACK_SHRINK, 0);
   alignment39->add(*hbox61);
#if GTK_VERSION_LT(3, 0)
   addDirectoryButton->set_flags(Gtk::CAN_FOCUS);
   addDirectoryButton->set_flags(Gtk::CAN_DEFAULT);
#else
   addDirectoryButton->set_can_focus();
   addDirectoryButton->set_can_default();
#endif
   addDirectoryButton->set_border_width(4);
   addDirectoryButton->set_relief(Gtk::RELIEF_NORMAL);
   addDirectoryButton->add(*alignment39);
#if GTK_VERSION_LT(3, 0)
   removeDirectoryButton->set_flags(Gtk::CAN_FOCUS);
   removeDirectoryButton->set_flags(Gtk::CAN_DEFAULT);
#else
   removeDirectoryButton->set_can_focus();
   removeDirectoryButton->set_can_default();
#endif
   removeDirectoryButton->set_border_width(4);
   removeDirectoryButton->set_relief(Gtk::RELIEF_NORMAL);
   vbuttonbox2->pack_start(*addDirectoryButton);
   vbuttonbox2->pack_start(*removeDirectoryButton);
   hbox60->pack_start(*scrolledwindow2);
   hbox60->pack_start(*vbuttonbox2, Gtk::PACK_SHRINK, 0);
   label76->set_alignment(0,0.5);
   label76->set_padding(4,4);
   label76->set_justify(Gtk::JUSTIFY_LEFT);
   label76->set_line_wrap(true);
   label76->set_use_markup(false);
   label76->set_selectable(false);
   label76->set_ellipsize(Pango::ELLIPSIZE_NONE);
   label76->set_width_chars(-1);
   label76->set_angle(0);
   label76->set_single_line_mode(false);
#if GTK_VERSION_LT(3, 0)
   patternsTreeview->set_flags(Gtk::CAN_FOCUS);
#else
   patternsTreeview->set_can_focus();
#endif
   patternsTreeview->set_headers_visible(true);
   patternsTreeview->set_rules_hint(false);
   patternsTreeview->set_reorderable(false);
   patternsTreeview->set_enable_search(true);
#if GTK_VERSION_LT(3, 0)
   scrolledwindow3->set_flags(Gtk::CAN_FOCUS);
#else
   scrolledwindow3->set_can_focus();
#endif
   scrolledwindow3->set_border_width(4);
   scrolledwindow3->set_shadow_type(Gtk::SHADOW_NONE);
   scrolledwindow3->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
   scrolledwindow3->property_window_placement().set_value(Gtk::CORNER_TOP_LEFT);
   scrolledwindow3->add(*patternsTreeview);
   vbox4->pack_start(*scrolledwindow3);
   vbox4->pack_start(*patternsCombobox, Gtk::PACK_SHRINK, 4);
   image728->set_alignment(0.5,0.5);
   image728->set_padding(0,0);
   label77->set_alignment(0.5,0.5);
   label77->set_padding(0,0);
   label77->set_justify(Gtk::JUSTIFY_LEFT);
   label77->set_line_wrap(false);
   label77->set_use_markup(false);
   label77->set_selectable(false);
   label77->set_ellipsize(Pango::ELLIPSIZE_NONE);
   label77->set_width_chars(-1);
   label77->set_angle(0);
   label77->set_single_line_mode(false);
   hbox63->pack_start(*image728, Gtk::PACK_SHRINK, 0);
   hbox63->pack_start(*label77, Gtk::PACK_SHRINK, 0);
   alignment40->add(*hbox63);
#if GTK_VERSION_LT(3, 0)
   addPatternButton->set_flags(Gtk::CAN_FOCUS);
   addPatternButton->set_flags(Gtk::CAN_DEFAULT);
#else
   addPatternButton->set_can_focus();
   addPatternButton->set_can_default();
#endif
   addPatternButton->set_border_width(4);
   addPatternButton->set_relief(Gtk::RELIEF_NORMAL);
   addPatternButton->add(*alignment40);
#if GTK_VERSION_LT(3, 0)
   removePatternButton->set_flags(Gtk::CAN_FOCUS);
   removePatternButton->set_flags(Gtk::CAN_DEFAULT);
#else
   removePatternButton->set_can_focus();
   removePatternButton->set_can_default();
#endif
   removePatternButton->set_border_width(4);
   removePatternButton->set_relief(Gtk::RELIEF_NORMAL);
#if GTK_VERSION_LT(3, 0)
   resetPatternsButton->set_flags(Gtk::CAN_FOCUS);
   resetPatternsButton->set_flags(Gtk::CAN_DEFAULT);
#else
   resetPatternsButton->set_can_focus();
   resetPatternsButton->set_can_default();
#endif
   resetPatternsButton->set_border_width(4);
   resetPatternsButton->set_relief(Gtk::RELIEF_NORMAL);
   vbuttonbox3->pack_start(*addPatternButton);
   vbuttonbox3->pack_start(*removePatternButton);
   vbuttonbox3->pack_start(*resetPatternsButton);
   hbox62->pack_start(*vbox4);
   hbox62->pack_start(*vbuttonbox3, Gtk::PACK_SHRINK, 0);
   vbox3->pack_start(*directoriesLabel, Gtk::PACK_SHRINK, 4);
   vbox3->pack_start(*hbox60, Gtk::PACK_EXPAND_WIDGET, 4);
   vbox3->pack_start(*label76, Gtk::PACK_SHRINK, 4);
   vbox3->pack_start(*hbox62, Gtk::PACK_EXPAND_WIDGET, 4);
   indexingLabel->set_alignment(0.5,0.5);
   indexingLabel->set_padding(0,0);
   indexingLabel->set_justify(Gtk::JUSTIFY_LEFT);
   indexingLabel->set_line_wrap(false);
   indexingLabel->set_use_markup(false);
   indexingLabel->set_selectable(false);
   indexingLabel->set_ellipsize(Pango::ELLIPSIZE_NONE);
   indexingLabel->set_width_chars(-1);
   indexingLabel->set_angle(0);
   indexingLabel->set_single_line_mode(false);
   indexLabelsLabel->set_alignment(0,0.5);
   indexLabelsLabel->set_padding(4,4);
   indexLabelsLabel->set_justify(Gtk::JUSTIFY_LEFT);
   indexLabelsLabel->set_line_wrap(true);
   indexLabelsLabel->set_use_markup(false);
   indexLabelsLabel->set_selectable(false);
   indexLabelsLabel->set_ellipsize(Pango::ELLIPSIZE_NONE);
   indexLabelsLabel->set_width_chars(-1);
   indexLabelsLabel->set_angle(0);
   indexLabelsLabel->set_single_line_mode(false);
#if GTK_VERSION_LT(3, 0)
   labelsTreeview->set_flags(Gtk::CAN_FOCUS);
#else
   labelsTreeview->set_can_focus();
#endif
   labelsTreeview->set_headers_visible(true);
   labelsTreeview->set_rules_hint(false);
   labelsTreeview->set_reorderable(false);
   labelsTreeview->set_enable_search(true);
#if GTK_VERSION_LT(3, 0)
   scrolledwindow1->set_flags(Gtk::CAN_FOCUS);
#else
   scrolledwindow1->set_can_focus();
#endif
   scrolledwindow1->set_border_width(4);
   scrolledwindow1->set_shadow_type(Gtk::SHADOW_NONE);
   scrolledwindow1->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
   scrolledwindow1->property_window_placement().set_value(Gtk::CORNER_TOP_LEFT);
   scrolledwindow1->add(*labelsTreeview);
   image725->set_alignment(0.5,0.5);
   image725->set_padding(0,0);
   label71->set_alignment(0.5,0.5);
   label71->set_padding(0,0);
   label71->set_justify(Gtk::JUSTIFY_LEFT);
   label71->set_line_wrap(false);
   label71->set_use_markup(false);
   label71->set_selectable(false);
   label71->set_ellipsize(Pango::ELLIPSIZE_NONE);
   label71->set_width_chars(-1);
   label71->set_angle(0);
   label71->set_single_line_mode(false);
   hbox58->pack_start(*image725, Gtk::PACK_SHRINK, 0);
   hbox58->pack_start(*label71, Gtk::PACK_SHRINK, 0);
   alignment37->add(*hbox58);
#if GTK_VERSION_LT(3, 0)
   addLabelButton->set_flags(Gtk::CAN_FOCUS);
   addLabelButton->set_flags(Gtk::CAN_DEFAULT);
#else
   addLabelButton->set_can_focus();
   addLabelButton->set_can_default();
#endif
   addLabelButton->set_border_width(4);
   addLabelButton->set_relief(Gtk::RELIEF_NORMAL);
   addLabelButton->add(*alignment37);
   image726->set_alignment(0.5,0.5);
   image726->set_padding(0,0);
   label72->set_alignment(0.5,0.5);
   label72->set_padding(0,0);
   label72->set_justify(Gtk::JUSTIFY_LEFT);
   label72->set_line_wrap(false);
   label72->set_use_markup(false);
   label72->set_selectable(false);
   label72->set_ellipsize(Pango::ELLIPSIZE_NONE);
   label72->set_width_chars(-1);
   label72->set_angle(0);
   label72->set_single_line_mode(false);
   hbox59->pack_start(*image726, Gtk::PACK_SHRINK, 0);
   hbox59->pack_start(*label72, Gtk::PACK_SHRINK, 0);
   alignment38->add(*hbox59);
#if GTK_VERSION_LT(3, 0)
   removeLabelButton->set_flags(Gtk::CAN_FOCUS);
   removeLabelButton->set_flags(Gtk::CAN_DEFAULT);
#else
   removeLabelButton->set_can_focus();
   removeLabelButton->set_can_default();
#endif
   removeLabelButton->set_border_width(4);
   removeLabelButton->set_relief(Gtk::RELIEF_NORMAL);
   removeLabelButton->add(*alignment38);
   vbuttonbox1->pack_start(*addLabelButton);
   vbuttonbox1->pack_start(*removeLabelButton);
   hbox57->pack_start(*scrolledwindow1);
   hbox57->pack_start(*vbuttonbox1, Gtk::PACK_SHRINK, 0);
   vbox2->pack_start(*indexLabelsLabel, Gtk::PACK_SHRINK, 4);
   vbox2->pack_start(*hbox57, Gtk::PACK_EXPAND_WIDGET, 4);
   labelsLabel->set_alignment(0.5,0.5);
   labelsLabel->set_padding(0,0);
   labelsLabel->set_justify(Gtk::JUSTIFY_LEFT);
   labelsLabel->set_line_wrap(false);
   labelsLabel->set_use_markup(false);
   labelsLabel->set_selectable(false);
   labelsLabel->set_ellipsize(Pango::ELLIPSIZE_NONE);
   labelsLabel->set_width_chars(-1);
   labelsLabel->set_angle(0);
   labelsLabel->set_single_line_mode(false);
#if GTK_VERSION_LT(3, 0)
   directConnectionRadiobutton->set_flags(Gtk::CAN_FOCUS);
#else
   directConnectionRadiobutton->set_can_focus();
#endif
   directConnectionRadiobutton->set_relief(Gtk::RELIEF_NORMAL);
   directConnectionRadiobutton->set_mode(true);
   directConnectionRadiobutton->set_active(true);
#if GTK_VERSION_LT(3, 0)
   proxyRadiobutton->set_flags(Gtk::CAN_FOCUS);
#else
   proxyRadiobutton->set_can_focus();
#endif
   proxyRadiobutton->set_relief(Gtk::RELIEF_NORMAL);
   proxyRadiobutton->set_mode(true);
   proxyRadiobutton->set_active(false);
   proxyAddressLabel->set_alignment(0,0.5);
   proxyAddressLabel->set_padding(0,0);
   proxyAddressLabel->set_justify(Gtk::JUSTIFY_LEFT);
   proxyAddressLabel->set_line_wrap(false);
   proxyAddressLabel->set_use_markup(false);
   proxyAddressLabel->set_selectable(false);
   proxyAddressLabel->set_ellipsize(Pango::ELLIPSIZE_NONE);
   proxyAddressLabel->set_width_chars(-1);
   proxyAddressLabel->set_angle(0);
   proxyAddressLabel->set_single_line_mode(false);
#if GTK_VERSION_LT(3, 0)
   proxyAddressEntry->set_flags(Gtk::CAN_FOCUS);
#else
   proxyAddressEntry->set_can_focus();
#endif
   proxyAddressEntry->set_visibility(true);
   proxyAddressEntry->set_editable(true);
   proxyAddressEntry->set_max_length(0);
   proxyAddressEntry->set_has_frame(true);
   proxyAddressEntry->set_activates_default(false);
   proxyTypeLabel->set_alignment(0,0.5);
   proxyTypeLabel->set_padding(0,0);
   proxyTypeLabel->set_justify(Gtk::JUSTIFY_LEFT);
   proxyTypeLabel->set_line_wrap(false);
   proxyTypeLabel->set_use_markup(false);
   proxyTypeLabel->set_selectable(false);
   proxyTypeLabel->set_ellipsize(Pango::ELLIPSIZE_NONE);
   proxyTypeLabel->set_width_chars(-1);
   proxyTypeLabel->set_angle(0);
   proxyTypeLabel->set_single_line_mode(false);
   proxyPortLabel->set_alignment(0,0.5);
   proxyPortLabel->set_padding(0,0);
   proxyPortLabel->set_justify(Gtk::JUSTIFY_LEFT);
   proxyPortLabel->set_line_wrap(false);
   proxyPortLabel->set_use_markup(false);
   proxyPortLabel->set_selectable(false);
   proxyPortLabel->set_ellipsize(Pango::ELLIPSIZE_NONE);
   proxyPortLabel->set_width_chars(-1);
   proxyPortLabel->set_angle(0);
   proxyPortLabel->set_single_line_mode(false);
#if GTK_VERSION_LT(3, 0)
   proxyPortSpinbutton->set_flags(Gtk::CAN_FOCUS);
#else
   proxyPortSpinbutton->set_can_focus();
#endif
   proxyPortSpinbutton->set_update_policy(Gtk::UPDATE_ALWAYS);
   proxyPortSpinbutton->set_numeric(false);
   proxyPortSpinbutton->set_digits(0);
   proxyPortSpinbutton->set_wrap(false);
   table3->set_row_spacings(0);
   table3->set_col_spacings(0);
   table3->attach(*proxyAddressLabel, 0, 1, 1, 2, Gtk::FILL, Gtk::FILL, 4, 4);
   table3->attach(*proxyAddressEntry, 1, 2, 1, 2, Gtk::EXPAND|Gtk::FILL, Gtk::FILL, 4, 4);
   table3->attach(*proxyTypeLabel, 0, 1, 0, 1, Gtk::FILL, Gtk::FILL, 4, 4);
   table3->attach(*proxyPortLabel, 0, 1, 2, 3, Gtk::FILL, Gtk::FILL, 4, 4);
   table3->attach(*proxyPortSpinbutton, 1, 2, 2, 3, Gtk::EXPAND|Gtk::FILL, Gtk::AttachOptions(), 4, 4);
   table3->attach(*proxyTypeCombobox, 1, 2, 0, 1, Gtk::EXPAND|Gtk::FILL, Gtk::AttachOptions(), 4, 4);
   table2->set_row_spacings(0);
   table2->set_col_spacings(0);
   table2->attach(*directConnectionRadiobutton, 0, 1, 0, 1, Gtk::EXPAND|Gtk::FILL, Gtk::FILL, 4, 4);
   table2->attach(*proxyRadiobutton, 0, 1, 1, 2, Gtk::EXPAND|Gtk::FILL, Gtk::FILL, 4, 4);
   table2->attach(*table3, 0, 1, 2, 3, Gtk::FILL, Gtk::EXPAND|Gtk::FILL, 0, 0);
   networkLabel->set_alignment(0.5,0.5);
   networkLabel->set_padding(0,0);
   networkLabel->set_justify(Gtk::JUSTIFY_LEFT);
   networkLabel->set_line_wrap(false);
   networkLabel->set_use_markup(false);
   networkLabel->set_selectable(false);
   networkLabel->set_ellipsize(Pango::ELLIPSIZE_NONE);
   networkLabel->set_width_chars(-1);
   networkLabel->set_angle(0);
   networkLabel->set_single_line_mode(false);
   apiKeyLabel->set_alignment(0,0.5);
   apiKeyLabel->set_padding(4,4);
   apiKeyLabel->set_justify(Gtk::JUSTIFY_LEFT);
   apiKeyLabel->set_line_wrap(false);
   apiKeyLabel->set_use_markup(false);
   apiKeyLabel->set_selectable(false);
   apiKeyLabel->set_ellipsize(Pango::ELLIPSIZE_NONE);
   apiKeyLabel->set_width_chars(-1);
   apiKeyLabel->set_angle(0);
   apiKeyLabel->set_single_line_mode(false);
#if GTK_VERSION_LT(3, 0)
   apiKeyEntry->set_flags(Gtk::CAN_FOCUS);
#else
   apiKeyEntry->set_can_focus();
#endif
   apiKeyEntry->set_visibility(true);
   apiKeyEntry->set_editable(true);
   apiKeyEntry->set_max_length(0);
   apiKeyEntry->set_has_frame(true);
   apiKeyEntry->set_activates_default(false);
   queriesLabel->set_alignment(0,0.5);
   queriesLabel->set_padding(4,4);
   queriesLabel->set_justify(Gtk::JUSTIFY_LEFT);
   queriesLabel->set_line_wrap(false);
   queriesLabel->set_use_markup(false);
   queriesLabel->set_selectable(false);
   queriesLabel->set_ellipsize(Pango::ELLIPSIZE_NONE);
   queriesLabel->set_width_chars(-1);
   queriesLabel->set_angle(0);
   queriesLabel->set_single_line_mode(false);
#if GTK_VERSION_LT(3, 0)
   enableCompletionCheckbutton->set_flags(Gtk::CAN_FOCUS);
#else
   enableCompletionCheckbutton->set_can_focus();
#endif
   enableCompletionCheckbutton->set_relief(Gtk::RELIEF_NORMAL);
   enableCompletionCheckbutton->set_mode(true);
   enableCompletionCheckbutton->set_active(false);
#if GTK_VERSION_LT(3, 0)
   newResultsColorbutton->set_flags(Gtk::CAN_FOCUS);
   ignoreRobotsCheckbutton->set_flags(Gtk::CAN_FOCUS);
#else
   newResultsColorbutton->set_can_focus();
   ignoreRobotsCheckbutton->set_can_focus();
#endif
   ignoreRobotsCheckbutton->set_relief(Gtk::RELIEF_NORMAL);
   ignoreRobotsCheckbutton->set_mode(true);
   ignoreRobotsCheckbutton->set_active(false);
   robotsLabels->set_alignment(0,0.5);
   robotsLabels->set_padding(4,4);
   robotsLabels->set_justify(Gtk::JUSTIFY_LEFT);
   robotsLabels->set_line_wrap(false);
   robotsLabels->set_use_markup(false);
   robotsLabels->set_selectable(false);
   robotsLabels->set_ellipsize(Pango::ELLIPSIZE_NONE);
   robotsLabels->set_width_chars(-1);
   robotsLabels->set_angle(0);
   robotsLabels->set_single_line_mode(false);
   newResultsLabel->set_alignment(0,0.5);
   newResultsLabel->set_padding(4,4);
   newResultsLabel->set_justify(Gtk::JUSTIFY_LEFT);
   newResultsLabel->set_line_wrap(false);
   newResultsLabel->set_use_markup(false);
   newResultsLabel->set_selectable(false);
   newResultsLabel->set_ellipsize(Pango::ELLIPSIZE_NONE);
   newResultsLabel->set_width_chars(-1);
   newResultsLabel->set_angle(0);
   newResultsLabel->set_single_line_mode(false);
   generalTable->set_row_spacings(0);
   generalTable->set_col_spacings(0);
   generalTable->attach(*apiKeyLabel, 0, 1, 3, 4, Gtk::FILL, Gtk::FILL, 0, 0);
   generalTable->attach(*apiKeyEntry, 1, 2, 3, 4, Gtk::EXPAND|Gtk::FILL, Gtk::FILL, 4, 4);
   generalTable->attach(*queriesLabel, 0, 1, 2, 3, Gtk::FILL, Gtk::FILL, 0, 0);
   generalTable->attach(*enableCompletionCheckbutton, 1, 2, 2, 3, Gtk::EXPAND|Gtk::FILL, Gtk::FILL, 4, 4);
   generalTable->attach(*newResultsColorbutton, 1, 2, 1, 2, Gtk::EXPAND|Gtk::FILL, Gtk::FILL, 4, 4);
   generalTable->attach(*ignoreRobotsCheckbutton, 1, 2, 0, 1, Gtk::EXPAND|Gtk::FILL, Gtk::FILL, 4, 4);
   generalTable->attach(*robotsLabels, 0, 1, 0, 1, Gtk::FILL, Gtk::AttachOptions(), 0, 0);
   generalTable->attach(*newResultsLabel, 0, 1, 1, 2, Gtk::FILL, Gtk::FILL, 0, 0);
   miscLabel->set_alignment(0.5,0.5);
   miscLabel->set_padding(0,0);
   miscLabel->set_justify(Gtk::JUSTIFY_LEFT);
   miscLabel->set_line_wrap(false);
   miscLabel->set_use_markup(false);
   miscLabel->set_selectable(false);
   miscLabel->set_ellipsize(Pango::ELLIPSIZE_NONE);
   miscLabel->set_width_chars(-1);
   miscLabel->set_angle(0);
   miscLabel->set_single_line_mode(false);
#if GTK_VERSION_LT(3, 0)
   prefsNotebook->set_flags(Gtk::CAN_FOCUS);
#else
   prefsNotebook->set_can_focus();
#endif
   prefsNotebook->set_show_tabs(true);
   prefsNotebook->set_show_border(true);
   prefsNotebook->set_tab_pos(Gtk::POS_TOP);
   prefsNotebook->set_scrollable(false);
   prefsNotebook->append_page(*vbox3, *indexingLabel);
#if GTK_VERSION_LT(3, 0)
   prefsNotebook->pages().back().set_tab_label_packing(false, true, Gtk::PACK_START);
#endif
   prefsNotebook->append_page(*vbox2, *labelsLabel);
#if GTK_VERSION_LT(3, 0)
   prefsNotebook->pages().back().set_tab_label_packing(false, true, Gtk::PACK_START);
#endif
   prefsNotebook->append_page(*table2, *networkLabel);
#if GTK_VERSION_LT(3, 0)
   prefsNotebook->pages().back().set_tab_label_packing(false, true, Gtk::PACK_START);
#endif
   prefsNotebook->append_page(*generalTable, *miscLabel);
#if GTK_VERSION_LT(3, 0)
   prefsNotebook->pages().back().set_tab_label_packing(false, true, Gtk::PACK_START);
#endif
   vbox1->set_border_width(4);
   vbox1->pack_start(*prefsNotebook);
   vbox1->pack_start(*hbuttonbox1, Gtk::PACK_SHRINK, 0);
   prefsWindow->set_title(_("Pinot Preferences"));
   prefsWindow->set_modal(false);
   prefsWindow->property_window_position().set_value(Gtk::WIN_POS_CENTER);
   prefsWindow->set_resizable(true);
   prefsWindow->property_destroy_with_parent().set_value(false);
   prefsWindow->add(*vbox1);
   prefsCancelbutton->show();
   prefsOkbutton->show();
   hbuttonbox1->show();
   directoriesLabel->show();
   directoriesTreeview->show();
   scrolledwindow2->show();
   image727->show();
   label75->show();
   hbox61->show();
   alignment39->show();
   addDirectoryButton->show();
   removeDirectoryButton->show();
   vbuttonbox2->show();
   hbox60->show();
   label76->show();
   patternsTreeview->show();
   scrolledwindow3->show();
   patternsCombobox->show();
   vbox4->show();
   image728->show();
   label77->show();
   hbox63->show();
   alignment40->show();
   addPatternButton->show();
   removePatternButton->show();
   resetPatternsButton->show();
   vbuttonbox3->show();
   hbox62->show();
   vbox3->show();
   indexingLabel->show();
   indexLabelsLabel->show();
   labelsTreeview->show();
   scrolledwindow1->show();
   image725->show();
   label71->show();
   hbox58->show();
   alignment37->show();
   addLabelButton->show();
   image726->show();
   label72->show();
   hbox59->show();
   alignment38->show();
   removeLabelButton->show();
   vbuttonbox1->show();
   hbox57->show();
   vbox2->show();
   labelsLabel->show();
   directConnectionRadiobutton->show();
   proxyRadiobutton->show();
   proxyAddressLabel->show();
   proxyAddressEntry->show();
   proxyTypeLabel->show();
   proxyPortLabel->show();
   proxyPortSpinbutton->show();
   proxyTypeCombobox->show();
   table3->show();
   table2->show();
   networkLabel->show();
   apiKeyLabel->show();
   apiKeyEntry->show();
   queriesLabel->show();
   enableCompletionCheckbutton->show();
   newResultsColorbutton->show();
   ignoreRobotsCheckbutton->show();
   robotsLabels->show();
   newResultsLabel->show();
   generalTable->show();
   miscLabel->show();
   prefsNotebook->show();
   vbox1->show();
   prefsWindow->show();
   prefsCancelbutton->signal_clicked().connect(sigc::mem_fun(*this, &prefsWindow_glade::on_prefsCancelbutton_clicked), false);
   prefsOkbutton->signal_clicked().connect(sigc::mem_fun(*this, &prefsWindow_glade::on_prefsOkbutton_clicked), false);
   addDirectoryButton->signal_clicked().connect(sigc::mem_fun(*this, &prefsWindow_glade::on_addDirectoryButton_clicked), false);
   removeDirectoryButton->signal_clicked().connect(sigc::mem_fun(*this, &prefsWindow_glade::on_removeDirectoryButton_clicked), false);
   patternsCombobox->signal_changed().connect(sigc::mem_fun(*this, &prefsWindow_glade::on_patternsCombobox_changed), false);
   addPatternButton->signal_clicked().connect(sigc::mem_fun(*this, &prefsWindow_glade::on_addPatternButton_clicked), false);
   removePatternButton->signal_clicked().connect(sigc::mem_fun(*this, &prefsWindow_glade::on_removePatternButton_clicked), false);
   resetPatternsButton->signal_clicked().connect(sigc::mem_fun(*this, &prefsWindow_glade::on_resetPatternsButton_clicked), false);
   addLabelButton->signal_clicked().connect(sigc::mem_fun(*this, &prefsWindow_glade::on_addLabelButton_clicked), false);
   removeLabelButton->signal_clicked().connect(sigc::mem_fun(*this, &prefsWindow_glade::on_removeLabelButton_clicked), false);
   directConnectionRadiobutton->signal_toggled().connect(sigc::mem_fun(*this, &prefsWindow_glade::on_directConnectionRadiobutton_toggled), false);
   prefsWindow->signal_delete_event().connect(sigc::mem_fun(*this, &prefsWindow_glade::on_prefsWindow_delete_event), false);
}

prefsWindow_glade::~prefsWindow_glade()
{  delete gmm_data;
}
