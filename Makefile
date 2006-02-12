
ROOT_DIR = .
include ${ROOT_DIR}/variables.mk

INSTALL_TARGETS = install-pinot
ifeq ($(HAS_GOOGLEAPI),yes)
SOAP_PROJECTS = Search/Google
endif
ifeq ($(HAS_OSAPI),yes)
SOAP_PROJECTS += Search/ObjectsSearch
endif
PROJECTS := Utils Tokenize SQL Collect ${SOAP_PROJECTS} Search Index \
	UI/RenderHTML UI/GTK2/src
TARGETS_ALL := $(patsubst %,%_all,${PROJECTS})
TARGETS_CLEAN := $(patsubst %,%_clean,${PROJECTS})

targets : ${TARGETS_ALL}

clean : ${TARGETS_CLEAN}

update :
	@cvs update

pinot_po :
	@xgettext -d pinot -o po/pinot.po --force-po --keyword=_ -f po/POTFILES
 
pinot_mo :
	@mkdir -p mo
	@msgfmt -o mo/en_GB.mo po/en_GB.po
	@msgfmt -o mo/fr_FR.mo po/fr_FR.po
	@msgfmt -o mo/es_ES.mo po/es_ES.po

%_clean :
	@make -C $(patsubst %_clean, %, $@) clean
	
%_all :
	@make -C $(patsubst %_all, %, $@) all

install: ${INSTALL_TARGETS}

install-pinot:
	@mkdir -p $(PREFIX)/usr/bin/
	install -m 755 ${BIN_DIR}/pinot $(PREFIX)/usr/bin/pinot
	install -m 755 ${BIN_DIR}/senginetest $(PREFIX)/usr/bin/pinot_search
	@mkdir -p $(PREFIX)/usr/share/pinot/engines/
	@mkdir -p $(PREFIX)/usr/share/pinot/tokenizers/
	install -m 644 index.html $(PREFIX)/usr/share/pinot/
	install -m 644 Search/Plugins/* $(PREFIX)/usr/share/pinot/engines/
	install -m 755 ${LIB_DIR}/*.so $(PREFIX)/usr/share/pinot/tokenizers/
	install -m 644 UI/GTK2/xapian-powered.png $(PREFIX)/usr/share/pinot/
	install -m 644 UI/GTK2/metase-gtk2.glade $(PREFIX)/usr/share/pinot/
	install -m 644 UI/GTK2/metase-gtk2.gladep $(PREFIX)/usr/share/pinot/
	install -m 644 textcat_conf.txt $(PREFIX)/usr/share/pinot/
	@mkdir -p $(PREFIX)/usr/share/locale/fr/LC_MESSAGES/
	install -m 644 mo/fr_FR.mo $(PREFIX)/usr/share/locale/fr/LC_MESSAGES/pinot.mo
	@mkdir -p $(PREFIX)/usr/share/locale/es/LC_MESSAGES/
	install -m 644 mo/es_ES.mo $(PREFIX)/usr/share/locale/es/LC_MESSAGES/pinot.mo
	@mkdir -p $(PREFIX)/usr/share/icons/hicolor/48x48/apps/
	install -m 644 UI/GTK2/pinot.png $(PREFIX)/usr/share/icons/hicolor/48x48/apps/

