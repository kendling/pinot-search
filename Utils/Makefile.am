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
	XapianDatabaseFactory.h

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
	XapianDatabaseFactory.cpp

AM_CXXFLAGS = -fPIC @HTTP_CFLAGS@ @INDEX_CFLAGS@ @MIME_CFLAGS@

