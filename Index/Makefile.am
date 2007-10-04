# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	DBusXapianIndex.h \
	FilterWrapper.h \
	IndexFactory.h \
	IndexInterface.h \
	LanguageDetector.h \
	XapianDatabase.h \
	XapianDatabaseFactory.h \
	XapianIndex.h

noinst_LTLIBRARIES = libIndex.la

libIndex_la_SOURCES = \
	DBusXapianIndex.cpp \
	FilterWrapper.cpp \
	IndexFactory.cpp \
	LanguageDetector.cpp \
	XapianDatabase.cpp \
	XapianDatabaseFactory.cpp \
	XapianIndex.cpp

bin_PROGRAMS = pinot-index pinot-label

pinot_index_LDADD = -L../Utils -L../Tokenize -L../Collect \
	-lIndex -lCollect -lTokenize -lBasicUtils -lUtils \
	@GLIBMM_LIBS@ @INDEX_LIBS@ @DBUS_LIBS@ @XML_LIBS@ @GMIME_LIBS@ @HTTP_LIBS@ @MISC_LIBS@

pinot_index_SOURCES = \
	pinot-index.cpp

pinot_index_DEPENDENCIES = libIndex.la

pinot_label_LDADD = -L../Utils -L../Tokenize \
	-lIndex -lTokenize -lBasicUtils -lUtils \
	@GLIBMM_LIBS@ @INDEX_LIBS@ @DBUS_LIBS@ @XML_LIBS@ @GMIME_LIBS@ @MISC_LIBS@

pinot_label_SOURCES = \
	pinot-label.cpp

pinot_label_DEPENDENCIES = libIndex.la

AM_CXXFLAGS = -I$(srcdir)/../Utils -I$(srcdir)/../Tokenize -I$(srcdir)/../Tokenize/filters \
	-I$(srcdir)/../Collect @HTTP_CFLAGS@ @GMIME_CFLAGS@ @XML_CFLAGS@ @DBUS_CFLAGS@ @INDEX_CFLAGS@

