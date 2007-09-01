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
	xesam/XapianQueryBuilder.h \
	xesam/XesamParser.h \
	xesam/XesamQLParser.h \
	xesam/XesamQueryBuilder.h \
	xesam/XesamULParser.h

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
	xesam/XapianQueryBuilder.cc \
	xesam/XesamQLParser.cc \
	xesam/XesamQueryBuilder.cc \
	xesam/XesamULParser.cc
if HAVE_BOOST_SPIRIT
libSearch_la_SOURCES += SherlockParser.cpp
endif

if HAVE_SOAP
libSearchSOAP_la_SOURCES = \
	SOAPEnvC.cpp \
	SOAPEnvNS.cpp
endif

bin_PROGRAMS = pinot-search

pinot_search_LDADD = -L../Utils -L../Tokenize -L../Collect -L../Index -LGoogle -Lxesam \
	@SEARCH_LIBS@ -lIndex -lCollect -lTokenize -lUtils -lBasicUtils \
	@GLIBMM_LIBS@ @INDEX_LIBS@ @SOAP_LIBS@ @XML_LIBS@ @GMIME_LIBS@ @HTTP_LIBS@ @MISC_LIBS@

pinot_search_SOURCES = \
	pinot-search.cpp

if HAVE_SOAP
pinot_search_DEPENDENCIES = libSearch.la libSearchSOAP.la
else
pinot_search_DEPENDENCIES = libSearch.la
endif

SOAPEnvC.cpp : SOAPEnv.h
	soapcpp2 -pSOAPEnv SOAPEnv.h

AM_CXXFLAGS = -I$(srcdir)/../Utils -I$(srcdir)/../Tokenize -I$(srcdir)/../Tokenize/filters \
	-I$(srcdir)/../Collect -I$(srcdir)/../Index -IGoogle -Ixesam \
	@HTTP_CFLAGS@ @XML_CFLAGS@ @INDEX_CFLAGS@ @SOAP_CFLAGS@ @GMIME_CFLAGS@ @GLIBMM_CFLAGS@
if HAVE_BOOST_SPIRIT
AM_CXXFLAGS += -DHAVE_BOOST_SPIRIT
endif

