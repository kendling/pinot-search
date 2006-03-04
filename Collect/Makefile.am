# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	CurlDownloader.h \
	DownloaderFactory.h \
	DownloaderInterface.h \
	FileCollector.h \
	MboxCollector.h \
	NeonDownloader.h \
	XapianCollector.h

noinst_LTLIBRARIES = libCollect.la

bin_PROGRAMS = pinot_collect

pinot_collect_SOURCES = \
	dloadtest.cpp

pinot_collect_LDADD = -L../Utils -L../Tokenize \
	-lCollect -lTokenize -lUtils \
	@INDEX_LIBS@ @XML_LIBS@ @MIME_LIBS@ @HTTP_LIBS@ @MISC_LIBS@

libCollect_la_SOURCES = \
	CurlDownloader.cpp \
	DownloaderFactory.cpp \
	DownloaderInterface.cpp \
	FileCollector.cpp \
	MboxCollector.cpp \
	NeonDownloader.cpp \
	XapianCollector.cpp

AM_CXXFLAGS = -I../Utils -I../Tokenize \
	@INDEX_CFLAGS@ @XML_CFLAGS@ @HTTP_CFLAGS@ @MIME_CFLAGS@

