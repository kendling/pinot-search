# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	OpenSearchParser.h \
	QueryProperties.h \
	SearchPluginProperties.h \
	WebEngine.h \
	SearchEngineFactory.h \
	XapianEngine.h \
	PluginWebEngine.h \
	SearchEngineInterface.h \
	SherlockParser.h

noinst_LTLIBRARIES = libSearch.la

libSearch_la_SOURCES = \
	OpenSearchParser.cpp \
	QueryProperties.cpp \
	SearchPluginProperties.cpp \
	WebEngine.cpp \
	SearchEngineFactory.cpp \
	XapianEngine.cpp \
	PluginWebEngine.cpp \
	SearchEngineInterface.cpp \
	SherlockParser.cpp

AM_CXXFLAGS = -I../Utils -I../Tokenize -I../Collect @XML_CFLAGS@

