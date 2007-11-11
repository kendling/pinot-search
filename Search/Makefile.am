# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	AbstractGenerator.h \
	OpenSearchParser.h \
	PluginParsers.h \
	PluginWebEngine.h \
	QueryProperties.h \
	ResultsExporter.h \
	SearchEngineFactory.h \
	SearchEngineInterface.h \
	SearchPluginProperties.h \
	SherlockParser.h \
	SOAPEnv.h \
	SOAPEnvH.h \
	SOAPEnvStub.h \
	WebEngine.h \
	XapianEngine.h \
	$(top_srcdir)/Search/xesam/XapianQueryBuilder.h \
	$(top_srcdir)/Search/xesam/XesamParser.h \
	$(top_srcdir)/Search/xesam/XesamQLParser.h \
	$(top_srcdir)/Search/xesam/XesamQueryBuilder.h \
	$(top_srcdir)/Search/xesam/XesamULParser.h

if HAVE_SOAP
noinst_LTLIBRARIES = libSearch.la libSearchSOAP.la
else
noinst_LTLIBRARIES = libSearch.la
endif

libSearch_la_SOURCES = \
	AbstractGenerator.cpp \
	OpenSearchParser.cpp \
	PluginWebEngine.cpp \
	QueryProperties.cpp \
	ResultsExporter.cpp \
	SearchEngineFactory.cpp \
	SearchEngineInterface.cpp \
	SearchPluginProperties.cpp \
	WebEngine.cpp \
	XapianEngine.cpp \
	$(top_srcdir)/Search/xesam/XapianQueryBuilder.cc \
	$(top_srcdir)/Search/xesam/XesamQLParser.cc \
	$(top_srcdir)/Search/xesam/XesamQueryBuilder.cc \
	$(top_srcdir)/Search/xesam/XesamULParser.cc

if HAVE_BOOST_SPIRIT
libSearch_la_SOURCES += SherlockParser.cpp
endif

if HAVE_SOAP
libSearchSOAP_la_SOURCES = SOAPEnvC.cpp SOAPEnvNS.cpp
endif

bin_PROGRAMS = pinot-search

pinot_search_LDADD = \
	-L$(top_builddir)/Utils \
	-L$(top_builddir)/Tokenize \
	-L$(top_builddir)/Collect \
	-L$(top_builddir)/Index \
	-L$(top_srcdir)/Search/Google \
	-L$(top_srcdir)/Search/xesam \
	@SEARCH_LIBS@ -lIndex -lCollect \
	-lTokenize -lUtils -lBasicUtils \
	@GLIBMM_LIBS@ @INDEX_LIBS@ @SOAP_LIBS@ \
	@XML_LIBS@ @GMIME_LIBS@ @HTTP_LIBS@ @MISC_LIBS@

pinot_search_SOURCES = \
	pinot-search.cpp

if HAVE_SOAP
pinot_search_DEPENDENCIES = libSearch.la libSearchSOAP.la
else
pinot_search_DEPENDENCIES = libSearch.la
endif

if HAVE_SOAP
SOAPEnvC.cpp : SOAPEnv.h
	soapcpp2 -pSOAPEnv SOAPEnv.h
endif

AM_CXXFLAGS = \
	-I$(top_srcdir)/Utils \
	-I$(top_srcdir)/Tokenize \
	-I$(top_srcdir)/Tokenize/filters \
	-I$(top_srcdir)/Collect \
	-I$(top_srcdir)/Index \
	-I$(top_srcdir)/Search/Google \
	-I$(top_srcdir)/Search/xesam \
	@HTTP_CFLAGS@ @XML_CFLAGS@ \
	@INDEX_CFLAGS@ @SOAP_CFLAGS@ @GMIME_CFLAGS@ @GLIBMM_CFLAGS@

if HAVE_BOOST_SPIRIT
AM_CXXFLAGS += -DHAVE_BOOST_SPIRIT
endif

