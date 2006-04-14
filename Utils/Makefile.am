# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	Document.h \
	HtmlDocument.h \
	Languages.h \
	MIMEScanner.h \
	NLS.h \
	StringManip.h \
	Timer.h \
	XapianDatabase.h \
	DocumentInfo.h \
	IndexedDocument.h \
	MboxParser.h \
	Result.h \
	TimeConverter.h \
	Url.h \
	XapianDatabaseFactory.h \
	xdgmime/xdgmimealias.h \
	xdgmime/xdgmimecache.h \
	xdgmime/xdgmimeglob.h \
	xdgmime/xdgmime.h \
	xdgmime/xdgmimeint.h \
	xdgmime/xdgmimemagic.h \
	xdgmime/xdgmimeparent.h

noinst_LTLIBRARIES = libUtils.la

libUtils_la_SOURCES = \
	Document.cpp \
	HtmlDocument.cpp \
	Languages.cpp \
	MIMEScanner.cpp \
	StringManip.cpp \
	Timer.cpp \
	XapianDatabase.cpp \
	DocumentInfo.cpp \
	IndexedDocument.cpp \
	MboxParser.cpp \
	Result.cpp \
	TimeConverter.cpp \
	Url.cpp \
	XapianDatabaseFactory.cpp \
	xdgmime/xdgmimealias.c \
	xdgmime/xdgmime.c \
	xdgmime/xdgmimecache.c \
	xdgmime/xdgmimeglob.c \
	xdgmime/xdgmimeint.c \
	xdgmime/xdgmimemagic.c \
	xdgmime/xdgmimeparent.c

AM_CFLAGS = -fPIC -DXDG_PREFIX=pinot_xdg
AM_CXXFLAGS = -fPIC @HTTP_CFLAGS@ @INDEX_CFLAGS@ @MIME_CFLAGS@ -DXDG_PREFIX=pinot_xdg

