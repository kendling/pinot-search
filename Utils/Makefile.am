# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	CommandLine.h \
	Document.h \
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

noinst_LTLIBRARIES = libBasicUtils.la libUtils.la

libBasicUtils_la_SOURCES = \
	CommandLine.cpp \
	Document.cpp \
	DocumentInfo.cpp \
	StringManip.cpp \
	TimeConverter.cpp \
	Url.cpp

libUtils_la_SOURCES = \
	IndexedDocument.cpp \
	Languages.cpp \
	MIMEScanner.cpp \
	MboxParser.cpp \
	Result.cpp \
	Timer.cpp \
	XapianDatabase.cpp \
	XapianDatabaseFactory.cpp \
	xdgmime/xdgmimealias.c \
	xdgmime/xdgmime.c \
	xdgmime/xdgmimecache.c \
	xdgmime/xdgmimeglob.c \
	xdgmime/xdgmimeint.c \
	xdgmime/xdgmimemagic.c \
	xdgmime/xdgmimeparent.c

AM_CFLAGS = -fPIC -DXDG_PREFIX=pinot_xdg
AM_CXXFLAGS = -fPIC @HTTP_CFLAGS@ @INDEX_CFLAGS@ @MIME_CFLAGS@ @GLIBMM_CFLAGS@ -DXDG_PREFIX=pinot_xdg

