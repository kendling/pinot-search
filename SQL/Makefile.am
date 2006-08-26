# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	CrawlHistory.h \
	QueryHistory.h \
	SQLiteBase.h \
	ViewHistory.h

noinst_LTLIBRARIES = libSQL.la

libSQL_la_SOURCES = \
	CrawlHistory.cpp \
	QueryHistory.cpp \
	SQLiteBase.cpp \
	ViewHistory.cpp

AM_CXXFLAGS = -I$(srcdir)/../Utils

