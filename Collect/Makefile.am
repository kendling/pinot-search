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

bin_PROGRAMS = pinot-collect

libCollect_la_SOURCES = \
	@HTTP_DOWNLOADER@.cpp \
	DownloaderFactory.cpp \
	DownloaderInterface.cpp \
	FileCollector.cpp \
	MboxCollector.cpp \
	XapianCollector.cpp

pinot_collect_SOURCES = \
	pinot-collect.cpp

pinot_collect_LDADD = -L../Utils -L../Tokenize \
	-lCollect -lTokenize -lUtils \
	@GLIBMM_LIBS@ @INDEX_LIBS@ @XML_LIBS@ @MIME_LIBS@ @HTTP_LIBS@ @MISC_LIBS@

AM_CXXFLAGS = -I$(srcdir)/../Utils -I$(srcdir)/../Tokenize \
	@INDEX_CFLAGS@ @XML_CFLAGS@ @HTTP_CFLAGS@ @MIME_CFLAGS@ @GLIBMM_CFLAGS@

