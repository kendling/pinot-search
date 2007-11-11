# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	ActionQueue.h \
	CrawlHistory.h \
	QueryHistory.h \
	SQLiteBase.h \
	ViewHistory.h

noinst_LTLIBRARIES = libSQL.la

libSQL_la_SOURCES = \
	ActionQueue.cpp \
	CrawlHistory.cpp \
	QueryHistory.cpp \
	SQLiteBase.cpp \
	ViewHistory.cpp

libSQL_la_CXXFLAGS = -I$(top_srcdir)/Utils

