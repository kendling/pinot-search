# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	DBusXapianIndex.h \
	IndexFactory.h \
	IndexInterface.h \
	LanguageDetector.h \
	WritableXapianIndex.h \
	XapianIndex.h

noinst_LTLIBRARIES = libIndex.la

bin_PROGRAMS = pinot-index

libIndex_la_SOURCES = \
	DBusXapianIndex.cpp \
	IndexFactory.cpp \
	LanguageDetector.cpp \
	WritableXapianIndex.cpp \
	XapianIndex.cpp

pinot_index_SOURCES = \
	pinot-index.cpp

pinot_index_LDADD = -L../Utils -L../Tokenize -L../Collect \
	-lIndex -lCollect -lTokenize -lUtils \
	@GLIBMM_LIBS@ @INDEX_LIBS@ @DBUS_LIBS@ @XML_LIBS@ @MIME_LIBS@ @HTTP_LIBS@ @MISC_LIBS@

AM_CXXFLAGS = -I$(srcdir)/../Utils -I$(srcdir)/../Tokenize -I$(srcdir)/../Collect \
	@HTTP_CFLAGS@ @MIME_CFLAGS@ @XML_CFLAGS@ @DBUS_CFLAGS@ @INDEX_CFLAGS@

