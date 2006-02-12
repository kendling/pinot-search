# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	DownloaderFactory.h \
	DownloaderInterface.h \
	FileCollector.h \
	MboxCollector.h \
	NeonDownloader.h \
	XapianCollector.h

noinst_LTLIBRARIES = libCollect.la

libCollect_la_SOURCES = \
	DownloaderFactory.cpp \
	DownloaderInterface.cpp \
	FileCollector.cpp \
	MboxCollector.cpp \
	NeonDownloader.cpp \
	XapianCollector.cpp

AM_CXXFLAGS = -I../Utils -I../Tokenize @HTTP_CFLAGS@ @MIME_CFLAGS@

