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

lib_LTLIBRARIES = libxapianmodule.la

libxapianmodule_la_SOURCES = \
	AbstractGenerator.cpp \
	LanguageDetector.cpp \
	ModuleExports.cc \
	XapianDatabase.cpp \
	XapianDatabaseFactory.cpp \
	XapianIndex.cpp \
	XapianEngine.cpp \
	$(top_srcdir)/IndexSearch/xesam/XapianQueryBuilder.cc \
	$(top_srcdir)/IndexSearch/xesam/XesamQLParser.cc \
	$(top_srcdir)/IndexSearch/xesam/XesamQueryBuilder.cc

if HAVE_BOOST_SPIRIT
libxapianmodule_la_SOURCES += $(top_srcdir)/IndexSearch/xesam/XesamULParser.cc
endif

libxapianmodule_la_LDFLAGS = -module -shared -nostartfiles @XML_CFLAGS@

libxapianmodule_la_LIBADD = \
	@INDEX_LIBS@ \
	-L$(top_builddir)/Utils \
	-lBasicUtils 
#-L$(top_srcdir)/Tokenize 
#-L$(top_builddir)/IndexSearch 
#-lTokenize -lUtils
## @GLIBMM_LIBS@ @INDEX_LIBS@ @XML_LIBS@ @MISC_LIBS@

AM_CXXFLAGS = \
	-fPIC \
	-I$(top_srcdir)/Utils \
	-I$(top_srcdir)/Tokenize \
	-I$(top_srcdir)/Tokenize/filters \
	-I$(top_srcdir)/IndexSearch \
	-I$(top_srcdir)/IndexSearch/xesam \
	@XML_CFLAGS@ @INDEX_CFLAGS@ @GLIBMM_CFLAGS@

if HAVE_BOOST_SPIRIT
AM_CXXFLAGS += -DHAVE_BOOST_SPIRIT
endif

