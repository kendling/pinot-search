# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	CurlDownloader.h \
	DownloaderFactory.h \
	DownloaderInterface.h \
	FileCollector.h \
	MboxCollector.h \
	NeonDownloader.h

noinst_LTLIBRARIES = libCollect.la

libCollect_la_SOURCES = \
	@HTTP_DOWNLOADER@.cpp \
	DownloaderFactory.cpp \
	DownloaderInterface.cpp \
	FileCollector.cpp \
	MboxCollector.cpp

AM_CXXFLAGS = -I$(srcdir)/../Utils -I$(srcdir)/../Tokenize -I$(srcdir)/../Tokenize/filters \
	@INDEX_CFLAGS@ @XML_CFLAGS@ @HTTP_CFLAGS@ @GMIME_CFLAGS@ @GLIBMM_CFLAGS@

