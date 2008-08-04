# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	ActionQueue.h \
	CrawlHistory.h \
	QueryHistory.h \
	SQLDB.h \
	SQLiteBase.h \
	ViewHistory.h

noinst_LTLIBRARIES = libSQL.la

libSQL_la_SOURCES = \
	ActionQueue.cpp \
	CrawlHistory.cpp \
	QueryHistory.cpp \
	SQLDB.cpp \
	SQLiteBase.cpp \
	ViewHistory.cpp

AM_CXXFLAGS = \
	@VISIB_CFLAGS@ \
	-I$(top_srcdir)/Utils

