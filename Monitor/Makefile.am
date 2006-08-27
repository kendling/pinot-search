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
	MonitorEvent.cpp \
	MonitorFactory.cpp \
	MonitorHandler.cpp
if HAVE_LINUX_INOTIFY
libMonitor_la_SOURCES += INotifyMonitor.cpp
endif

AM_CXXFLAGS = -I$(srcdir)/../Utils -I$(srcdir)/../Tokenize -I$(srcdir)/../SQL \
	-I$(srcdir)/../Collect -I$(srcdir)/../Search -I$(srcdir)/../Index \
	@HTTP_CFLAGS@ @XML_CFLAGS@ @INDEX_CFLAGS@ @SOAP_CFLAGS@ @MIME_CFLAGS@ @SIGCPP_CFLAGS@
if HAVE_LINUX_INOTIFY
AM_CXXFLAGS += -DHAVE_LINUX_INOTIFY
endif

