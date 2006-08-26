# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	AbstractGenerator.h \
	OpenSearchParser.h \
	QueryProperties.h \
	SearchPluginProperties.h \
	WebEngine.h \
	SearchEngineFactory.h \
	XapianEngine.h \
	PluginParsers.h \
	PluginWebEngine.h \
	SearchEngineInterface.h \
	SherlockParser.h \
	SOAPEnv.h \
	SOAPEnvH.h \
	SOAPEnvStub.h

EXTRA_LTLIBRARIES = libSearch.la libSearchSOAP.la

noinst_LTLIBRARIES = @SEARCH_LTLIBRARIES@

bin_PROGRAMS = pinot-search

libSearch_la_SOURCES = \
	AbstractGenerator.cpp \
	OpenSearchParser.cpp \
	QueryProperties.cpp \
	SearchPluginProperties.cpp \
	WebEngine.cpp \
	SearchEngineFactory.cpp \
	XapianEngine.cpp \
	PluginWebEngine.cpp \
	SearchEngineInterface.cpp
if HAVE_BOOST_SPIRIT
libSearch_la_SOURCES += SherlockParser.cpp
endif

libSearchSOAP_la_SOURCES = \
	SOAPEnvC.cpp \
	SOAPEnvNS.cpp

pinot_search_SOURCES = \
	pinot-search.cpp

pinot_search_LDADD = -L../Utils -L../Tokenize -L../Collect -LGoogle \
	@SEARCH_LIBS@ -lCollect -lTokenize -lUtils \
	@GLIBMM_LIBS@ @INDEX_LIBS@ @SOAP_LIBS@ @XML_LIBS@ @MIME_LIBS@ @HTTP_LIBS@ @MISC_LIBS@

SOAPEnvC.cpp : SOAPEnv.h
	soapcpp2 -pSOAPEnv SOAPEnv.h

AM_CXXFLAGS = -I$(srcdir)/../Utils -I$(srcdir)/../Tokenize -I$(srcdir)/../Collect -IGoogle \
	@HTTP_CFLAGS@ @XML_CFLAGS@ @INDEX_CFLAGS@ @SOAP_CFLAGS@ @MIME_CFLAGS@ @GLIBMM_CFLAGS@

