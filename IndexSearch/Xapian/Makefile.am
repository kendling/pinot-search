# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	AbstractGenerator.h \
	LanguageDetector.h \
	XapianDatabase.h \
	XapianDatabaseFactory.h \
	XapianIndex.h \
	XapianEngine.h \
	$(top_srcdir)/IndexSearch/xesam/XapianQueryBuilder.h \
	$(top_srcdir)/IndexSearch/xesam/XesamParser.h \
	$(top_srcdir)/IndexSearch/xesam/XesamQLParser.h \
	$(top_srcdir)/IndexSearch/xesam/XesamQueryBuilder.h \
	$(top_srcdir)/IndexSearch/xesam/XesamULParser.h

noinst_LTLIBRARIES = libIndexSearchXapian.la

libIndexSearchXapian_la_SOURCES = \
	AbstractGenerator.cpp \
	LanguageDetector.cpp \
	XapianDatabase.cpp \
	XapianDatabaseFactory.cpp \
	XapianIndex.cpp \
	XapianEngine.cpp \
	$(top_srcdir)/IndexSearch/xesam/XapianQueryBuilder.cc \
	$(top_srcdir)/IndexSearch/xesam/XesamQLParser.cc \
	$(top_srcdir)/IndexSearch/xesam/XesamQueryBuilder.cc

if HAVE_BOOST_SPIRIT
libIndexSearchXapian_la_SOURCES += $(top_srcdir)/IndexSearch/xesam/XesamULParser.cc
endif

AM_CXXFLAGS = \
	-I$(top_srcdir)/Utils \
	-I$(top_srcdir)/Tokenize \
	-I$(top_srcdir)/Tokenize/filters \
	-I$(top_srcdir)/Collect \
	-I$(top_srcdir)/IndexSearch \
	-I$(top_srcdir)/IndexSearch/xesam \
	@HTTP_CFLAGS@ @GMIME_CFLAGS@ @XML_CFLAGS@ \
	@INDEX_CFLAGS@ @GLIBMM_CFLAGS@

if HAVE_BOOST_SPIRIT
AM_CXXFLAGS += -DHAVE_BOOST_SPIRIT
endif

