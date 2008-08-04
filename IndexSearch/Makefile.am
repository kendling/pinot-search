# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	DBusIndex.h \
	FilterWrapper.h \
	IndexInterface.h \
	ModuleFactory.h \
	OpenSearchParser.h \
	PluginParsers.h \
	PluginWebEngine.h \
	QueryProperties.h \
	ResultsExporter.h \
	SearchEngineInterface.h \
	SearchPluginProperties.h \
	SherlockParser.h \
	WebEngine.h \
	XesamLog.h

noinst_LTLIBRARIES = libIndex.la libIndexSearch.la

libIndex_la_SOURCES = \
	DBusIndex.cpp

libIndexSearch_la_SOURCES = \
	DBusIndex.cpp \
	FilterWrapper.cpp \
	ModuleFactory.cpp \
	OpenSearchParser.cpp \
	PluginWebEngine.cpp \
	QueryProperties.cpp \
	ResultsExporter.cpp \
	SearchEngineInterface.cpp \
	SearchPluginProperties.cpp \
	WebEngine.cpp

if HAVE_BOOST_SPIRIT
libIndexSearch_la_SOURCES += SherlockParser.cpp
endif

bin_PROGRAMS = pinot-index pinot-label pinot-search

pinot_index_LDFLAGS = \
	-rdynamic

pinot_index_LDADD = \
	-L$(top_builddir)/Utils \
	-L$(top_builddir)/Tokenize \
	-L$(top_builddir)/Collect \
	-L$(top_builddir)/IndexSearch/Google \
	-lIndexSearch -lCollect -lTokenize -lUtils -lBasicUtils \
	@GLIBMM_LIBS@ @DBUS_LIBS@ \
	@XML_LIBS@ @HTTP_LIBS@ @MISC_LIBS@

pinot_index_SOURCES = pinot-index.cpp

pinot_index_DEPENDENCIES = libIndexSearch.la

pinot_label_LDFLAGS = \
	-rdynamic

pinot_label_LDADD = \
	-L$(top_builddir)/Utils \
	-lIndex -lUtils -lBasicUtils \
	@GLIBMM_LIBS@ @DBUS_LIBS@ @GTHREAD_LIBS@ @XML_LIBS@

pinot_label_SOURCES = pinot-label.cpp

pinot_label_DEPENDENCIES = libIndex.la

pinot_search_LDFLAGS = \
	-rdynamic

pinot_search_LDADD = \
	-L$(top_builddir)/Utils \
	-L$(top_builddir)/Tokenize \
	-L$(top_builddir)/Collect \
	-L$(top_builddir)/IndexSearch/Google \
	-L$(top_builddir)/IndexSearch/xesam \
	-lIndexSearch -lCollect -lTokenize -lUtils -lBasicUtils \
	@GLIBMM_LIBS@ @DBUS_LIBS@ \
	@XML_LIBS@ @HTTP_LIBS@ @MISC_LIBS@

pinot_search_SOURCES = \
	pinot-search.cpp

pinot_search_DEPENDENCIES = libIndexSearch.la

AM_CXXFLAGS = \
	@VISIB_CFLAGS@ \
	-I$(top_srcdir)/Utils \
	-I$(top_srcdir)/Tokenize \
	-I$(top_srcdir)/Tokenize/filters \
	-I$(top_srcdir)/Collect \
	-I$(top_srcdir)/IndexSearch/cjkv \
	-I$(top_srcdir)/IndexSearch/Google \
	-I$(top_srcdir)/IndexSearch/xesam \
	@HTTP_CFLAGS@ @XML_CFLAGS@ @DBUS_CFLAGS@ \
	@INDEX_CFLAGS@ @GLIBMM_CFLAGS@

if HAVE_BOOST_SPIRIT
AM_CXXFLAGS += -DHAVE_BOOST_SPIRIT
endif

