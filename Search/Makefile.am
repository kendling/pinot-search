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
	SherlockParser.h

noinst_LTLIBRARIES = libSearch.la

bin_PROGRAMS = pinot_search

libSearch_la_SOURCES = \
	AbstractGenerator.cpp \
	OpenSearchParser.cpp \
	QueryProperties.cpp \
	SearchPluginProperties.cpp \
	WebEngine.cpp \
	SearchEngineFactory.cpp \
	XapianEngine.cpp \
	PluginWebEngine.cpp \
	SearchEngineInterface.cpp \
	SherlockParser.cpp

pinot_search_SOURCES = \
	senginetest.cpp

pinot_search_LDADD = -L../Utils -L../Tokenize -L../Collect \
	-lSearch -lCollect -lTokenize -lUtils \
	@INDEX_LIBS@ @XML_LIBS@ @MIME_LIBS@ @HTTP_LIBS@ @MISC_LIBS@

AM_CXXFLAGS = -I../Utils -I../Tokenize -I../Collect \
	@HTTP_CFLAGS@ @XML_CFLAGS@ @INDEX_CFLAGS@ @MIME_CFLAGS@

