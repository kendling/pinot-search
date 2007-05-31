
SUBDIRS = po Utils Tokenize SQL Collect Index @SOAP_SUBDIRS@ Search Monitor \
	UI/GTK2/src

EXTRA_DIST = AUTHORS ChangeLog NEWS README TODO mkinstalldirs \
	Tokenize/filters/external-filters.xml globalconfig.xml \
	textcat_conf.txt textcat3_conf.txt \
	pinot.desktop pinot-dbus-daemon.desktop pinot.spec \
	Search/Plugins/*src Search/Plugins/*.xml \
	Index/pinot-index.1 Search/pinot-search.1 \
	UI/GTK2/src/pinot.1 UI/GTK2/src/pinot-dbus-daemon.1 \
	UI/GTK2/src/pinot-dbus-daemon.xml UI/GTK2/src/de.berlios.Pinot.service \
	UI/GTK2/xapian-powered.png UI/icons/48x48/pinot.png \
	UI/icons/32x32/pinot.png UI/icons/24x24/pinot.png \
	UI/icons/22x22/pinot.png UI/icons/16x16/pinot.png \
	UI/GTK2/metase-gtk2.glade UI/GTK2/metase-gtk2.gladep \
	scripts/bash/pinot-enum-index.sh scripts/python/pinot-live.py

man_MANS = Index/pinot-index.1 Search/pinot-search.1 \
	UI/GTK2/src/pinot.1 UI/GTK2/src/pinot-dbus-daemon.1

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/pinot
	$(INSTALL_DATA) Tokenize/filters/external-filters.xml $(DESTDIR)$(sysconfdir)/pinot/external-filters.xml
	$(INSTALL_DATA) globalconfig.xml $(DESTDIR)$(sysconfdir)/pinot/globalconfig.xml
	$(INSTALL_DATA) textcat_conf.txt $(DESTDIR)$(sysconfdir)/pinot/textcat_conf.txt
	$(INSTALL_DATA) textcat3_conf.txt $(DESTDIR)$(sysconfdir)/pinot/textcat3_conf.txt
	$(mkinstalldirs) $(DESTDIR)$(datadir)/pinot $(DESTDIR)$(datadir)/dbus-1/services
	$(INSTALL_DATA) UI/GTK2/xapian-powered.png $(DESTDIR)$(datadir)/pinot/xapian-powered.png
	$(INSTALL_DATA) UI/GTK2/metase-gtk2.glade $(DESTDIR)$(datadir)/pinot/metase-gtk2.glade
	$(INSTALL_DATA) UI/GTK2/metase-gtk2.gladep $(DESTDIR)$(datadir)/pinot/metase-gtk2.gladep
	$(INSTALL_DATA) UI/GTK2/src/pinot-dbus-daemon.xml $(DESTDIR)$(datadir)/pinot/pinot-dbus-daemon.xml
	$(INSTALL_DATA) UI/GTK2/src/de.berlios.Pinot.service $(DESTDIR)$(datadir)/dbus-1/services/de.berlios.Pinot.service
	$(mkinstalldirs) $(DESTDIR)$(datadir)/pinot/engines
	$(INSTALL_DATA) Search/Plugins/*.src $(DESTDIR)$(datadir)/pinot/engines/
	@mv $(DESTDIR)$(datadir)/pinot/engines/AmazonAPI.src $(DESTDIR)$(datadir)/pinot/
	$(INSTALL_DATA) Search/Plugins/*.xml $(DESTDIR)$(datadir)/pinot/engines/
	$(mkinstalldirs) $(DESTDIR)$(libdir)/pinot/filters
	@rm $(DESTDIR)$(libdir)/lib*filter.a $(DESTDIR)$(libdir)/lib*filter.la
	@mv $(DESTDIR)$(libdir)/lib*filter* $(DESTDIR)$(libdir)/pinot/filters/
	$(mkinstalldirs) $(DESTDIR)$(datadir)/icons/hicolor/48x48/apps/
	$(INSTALL_DATA) UI/icons/48x48/pinot.png $(DESTDIR)$(datadir)/icons/hicolor/48x48/apps/pinot.png
	$(mkinstalldirs) $(DESTDIR)$(datadir)/icons/hicolor/32x32/apps/
	$(INSTALL_DATA) UI/icons/32x32/pinot.png $(DESTDIR)$(datadir)/icons/hicolor/32x32/apps/pinot.png
	$(mkinstalldirs) $(DESTDIR)$(datadir)/icons/hicolor/24x24/apps/
	$(INSTALL_DATA) UI/icons/24x24/pinot.png $(DESTDIR)$(datadir)/icons/hicolor/24x24/apps/pinot.png
	$(mkinstalldirs) $(DESTDIR)$(datadir)/icons/hicolor/22x22/apps/
	$(INSTALL_DATA) UI/icons/22x22/pinot.png $(DESTDIR)$(datadir)/icons/hicolor/22x22/apps/pinot.png
	$(mkinstalldirs) $(DESTDIR)$(datadir)/icons/hicolor/16x16/apps/
	$(INSTALL_DATA) UI/icons/16x16/pinot.png $(DESTDIR)$(datadir)/icons/hicolor/16x16/apps/pinot.png
	$(mkinstalldirs) $(DESTDIR)$(datadir)/applications
	@desktop-file-install --vendor="" --dir=$(DESTDIR)$(datadir)/applications pinot.desktop
	$(mkinstalldirs) $(DESTDIR)${sysconfdir}/xdg/autostart
	@desktop-file-install --vendor="" --dir=$(DESTDIR)${sysconfdir}/xdg/autostart pinot-dbus-daemon.desktop
	$(mkinstalldirs) $(DESTDIR)$(libdir)/deskbar-applet/handlers/
	$(INSTALL_DATA) scripts/python/pinot-live.py $(DESTDIR)$(libdir)/deskbar-applet/handlers/

uninstall-local:
	@rm -rf $(DESTDIR)$(sysconfdir)/pinot
	@rm -rf $(DESTDIR)$(datadir)/pinot
	@rm -rf $(DESTDIR)$(datadir)/dbus-1/services/de.berlios.Pinot.service
	@rm -rf $(DESTDIR)$(libdir)/pinot
	@rm -rf $(DESTDIR)$(datadir)/icons/hicolor/48x48/apps/pinot.png
	@rm -rf $(DESTDIR)$(datadir)/icons/hicolor/32x32/apps/pinot.png
	@rm -rf $(DESTDIR)$(datadir)/icons/hicolor/24x24/apps/pinot.png
	@rm -rf $(DESTDIR)$(datadir)/icons/hicolor/22x22/apps/pinot.png
	@rm -rf $(DESTDIR)$(datadir)/icons/hicolor/16x16/apps/pinot.png
	@rm -rf $(DESTDIR)$(datadir)/applications/pinot.desktop
	@rm -rf $(DESTDIR)${sysconfdir}/xdg/autostart/pinot-dbus-daemon.desktop
	@rm -rf $(DESTDIR)$(libdir)/deskbar-applet/handlers/pinot-live.py*

ACLOCAL_AMFLAGS = -I m4

