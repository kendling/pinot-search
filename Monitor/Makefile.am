# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	INotifyMonitor.h \
	MonitorEvent.h \
	MonitorFactory.h \
	MonitorHandler.h \
	MonitorInterface.h \
	linux-inotify-syscalls.h

noinst_LTLIBRARIES = libMonitor.la

libMonitor_la_SOURCES = \
	INotifyMonitor.cpp \
	MonitorEvent.cpp \
	MonitorFactory.cpp \
	MonitorHandler.cpp

AM_CXXFLAGS = -I../Utils -I../Tokenize -I../SQL -I../Collect -I../Search -I../Index \
	@HTTP_CFLAGS@ @XML_CFLAGS@ @INDEX_CFLAGS@ @SOAP_CFLAGS@ @MIME_CFLAGS@ @SIGCPP_CFLAGS@

