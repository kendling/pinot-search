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

lib_LTLIBRARIES = libxapianbackend.la

libxapianbackend_la_SOURCES = \
	AbstractGenerator.cpp \
	LanguageDetector.cpp \
	ModuleExports.cpp \
	XapianDatabase.cpp \
	XapianDatabaseFactory.cpp \
	XapianIndex.cpp \
	XapianEngine.cpp \
	$(top_srcdir)/IndexSearch/xesam/XapianQueryBuilder.cc \
	$(top_srcdir)/IndexSearch/xesam/XesamQLParser.cc \
	$(top_srcdir)/IndexSearch/xesam/XesamQueryBuilder.cc

if HAVE_BOOST_SPIRIT
libxapianbackend_la_SOURCES += $(top_srcdir)/IndexSearch/xesam/XesamULParser.cc
endif

libxapianbackend_la_LDFLAGS = -module -shared -nostartfiles

libxapianbackend_la_LIBADD = \
	-L$(top_builddir)/Utils \
	-lBasicUtils \
	@XML_LIBS@ @HTTP_LIBS@ \
	@INDEX_LIBS@ @MISC_LIBS@

AM_CXXFLAGS = \
	@MISC_CFLAGS@ \
	-I$(top_srcdir)/Utils \
	-I$(top_srcdir)/Tokenize \
	-I$(top_srcdir)/Tokenize/filters \
	-I$(top_srcdir)/IndexSearch \
	-I$(top_srcdir)/IndexSearch/cjkv \
	-I$(top_srcdir)/IndexSearch/xesam \
	@HTTP_CFLAGS@ @XML_CFLAGS@ @INDEX_CFLAGS@ @GLIBMM_CFLAGS@

if HAVE_BOOST_SPIRIT
AM_CXXFLAGS += -DHAVE_BOOST_SPIRIT
endif

