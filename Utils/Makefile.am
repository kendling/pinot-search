# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	CommandLine.h \
	Document.h \
	DocumentInfo.h \
	Languages.h \
	MIMEScanner.h \
	Memory.h \
	NLS.h \
	StringManip.h \
	TimeConverter.h \
	Timer.h \
	Url.h \
	Visibility.h \
	$(top_srcdir)/Utils/unac/unac.h \
	$(top_srcdir)/Utils/xdgmime/xdgmimealias.h \
	$(top_srcdir)/Utils/xdgmime/xdgmimecache.h \
	$(top_srcdir)/Utils/xdgmime/xdgmimeglob.h \
	$(top_srcdir)/Utils/xdgmime/xdgmime.h \
	$(top_srcdir)/Utils/xdgmime/xdgmimeint.h \
	$(top_srcdir)/Utils/xdgmime/xdgmimemagic.h \
	$(top_srcdir)/Utils/xdgmime/xdgmimeparent.h

if HAVE_GIO_MIME
noinst_HEADERS += \
	$(top_srcdir)/Utils/xdgmime/xdgmimealias.c \
	$(top_srcdir)/Utils/xdgmime/xdgmime.c \
	$(top_srcdir)/Utils/xdgmime/xdgmimecache.c \
	$(top_srcdir)/Utils/xdgmime/xdgmimeglob.c \
	$(top_srcdir)/Utils/xdgmime/xdgmimeint.c \
	$(top_srcdir)/Utils/xdgmime/xdgmimemagic.c \
	$(top_srcdir)/Utils/xdgmime/xdgmimeparent.c
endif

noinst_LTLIBRARIES = libBasicUtils.la libUtils.la

libBasicUtils_la_SOURCES = \
	CommandLine.cpp \
	Document.cpp \
	DocumentInfo.cpp \
	StringManip.cpp \
	TimeConverter.cpp \
	Timer.cpp \
	Url.cpp \
	$(top_srcdir)/Utils/unac/unac.c

libUtils_la_SOURCES = \
	Languages.cpp \
	MIMEScanner.cpp \
	Memory.cpp

if HAVE_GIO_MIME
else
libUtils_la_SOURCES += \
	$(top_srcdir)/Utils/xdgmime/xdgmimealias.c \
	$(top_srcdir)/Utils/xdgmime/xdgmime.c \
	$(top_srcdir)/Utils/xdgmime/xdgmimecache.c \
	$(top_srcdir)/Utils/xdgmime/xdgmimeglob.c \
	$(top_srcdir)/Utils/xdgmime/xdgmimeint.c \
	$(top_srcdir)/Utils/xdgmime/xdgmimemagic.c \
	$(top_srcdir)/Utils/xdgmime/xdgmimeparent.c
endif

AM_CFLAGS = -fPIC -DXDG_PREFIX=pinot_xdg -DUNAC_VERSION=\"1.0.7\"
AM_CXXFLAGS = \
	@MISC_CFLAGS@ \
	@HTTP_CFLAGS@ @INDEX_CFLAGS@ @GLIBMM_CFLAGS@ -DXDG_PREFIX=pinot_xdg -DUNAC_VERSION=\"1.0.7\"

