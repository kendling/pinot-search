# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	CommandLine.h \
	Document.h \
	DocumentInfo.h \
	Languages.h \
	MIMEScanner.h \
	NLS.h \
	StringManip.h \
	TimeConverter.h \
	Timer.h \
	Url.h \
	$(top_srcdir)/Utils/xdgmime/xdgmimealias.h \
	$(top_srcdir)/Utils/xdgmime/xdgmimecache.h \
	$(top_srcdir)/Utils/xdgmime/xdgmimeglob.h \
	$(top_srcdir)/Utils/xdgmime/xdgmime.h \
	$(top_srcdir)/Utils/xdgmime/xdgmimeint.h \
	$(top_srcdir)/Utils/xdgmime/xdgmimemagic.h \
	$(top_srcdir)/Utils/xdgmime/xdgmimeparent.h

noinst_LTLIBRARIES = libBasicUtils.la libUtils.la

libBasicUtils_la_SOURCES = \
	CommandLine.cpp \
	Document.cpp \
	DocumentInfo.cpp \
	StringManip.cpp \
	TimeConverter.cpp \
	Url.cpp

libUtils_la_SOURCES = \
	Languages.cpp \
	MIMEScanner.cpp \
	Timer.cpp \
	$(top_srcdir)/Utils/xdgmime/xdgmimealias.c \
	$(top_srcdir)/Utils/xdgmime/xdgmime.c \
	$(top_srcdir)/Utils/xdgmime/xdgmimecache.c \
	$(top_srcdir)/Utils/xdgmime/xdgmimeglob.c \
	$(top_srcdir)/Utils/xdgmime/xdgmimeint.c \
	$(top_srcdir)/Utils/xdgmime/xdgmimemagic.c \
	$(top_srcdir)/Utils/xdgmime/xdgmimeparent.c

AM_CFLAGS = -fPIC -DXDG_PREFIX=pinot_xdg
AM_CXXFLAGS = -fPIC @HTTP_CFLAGS@ @INDEX_CFLAGS@ @GMIME_CFLAGS@ @GLIBMM_CFLAGS@ -DXDG_PREFIX=pinot_xdg

