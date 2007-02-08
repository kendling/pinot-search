
SUBDIRS = po Utils Tokenize SQL Collect Index @SOAP_SUBDIRS@ Search Monitor UI/GTK2/src

EXTRA_DIST = ChangeLog NEWS README TODO mkinstalldirs \
	globalconfig.xml textcat_conf.txt textcat3_conf.txt \
	pinot.desktop pinot-dbus-daemon.desktop pinot.spec \
	Search/Plugins/*src Search/Plugins/*.xml \
	Collect/pinot-collect.1 Index/pinot-index.1 Search/pinot-search.1 \
	UI/GTK2/src/pinot.1 UI/GTK2/src/pinot-dbus-daemon.1 \
	UI/GTK2/src/pinot-dbus-daemon.xml UI/GTK2/src/de.berlios.Pinot.service \
	UI/GTK2/xapian-powered.png UI/GTK2/pinot.png \
	UI/GTK2/metase-gtk2.glade UI/GTK2/metase-gtk2.gladep \
	scripts/bash/pinot-enum-index.sh scripts/python/pinot-live.py

man_MANS = Collect/pinot-collect.1 Index/pinot-index.1 Search/pinot-search.1 \
	UI/GTK2/src/pinot.1 UI/GTK2/src/pinot-dbus-daemon.1

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/pinot
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
	$(mkinstalldirs) $(DESTDIR)$(libdir)/pinot/tokenizers
	@rm $(DESTDIR)$(libdir)/lib*tokenizer.a $(DESTDIR)$(libdir)/lib*tokenizer.la
	@mv $(DESTDIR)$(libdir)/lib*tokenizer* $(DESTDIR)$(libdir)/pinot/tokenizers/
	$(mkinstalldirs) $(DESTDIR)$(datadir)/icons/hicolor/48x48/apps/
	$(INSTALL_DATA) UI/GTK2/pinot.png $(DESTDIR)$(datadir)/icons/hicolor/48x48/apps/pinot.png
	$(mkinstalldirs) $(DESTDIR)$(datadir)/applications
	@desktop-file-install --vendor="" --dir=$(DESTDIR)$(datadir)/applications pinot.desktop
	$(mkinstalldirs) $(DESTDIR)${sysconfdir}/xdg/autostart
	@desktop-file-install --vendor="" --dir=$(DESTDIR)${sysconfdir}/xdg/autostart pinot-dbus-daemon.desktop
	$(mkinstalldirs) $(DESTDIR)$(libdir)/deskbar-applet/handlers/
	$(INSTALL_DATA) scripts/python/pinot-live.py $(DESTDIR)$(libdir)/deskbar-applet/handlers/

ACLOCAL_AMFLAGS = -I m4

