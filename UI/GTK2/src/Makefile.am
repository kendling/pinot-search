# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	importDialog_glade.hh \
	importDialog.hh \
	indexDialog_glade.hh \
	indexDialog.hh \
	launcherDialog.hh \
	launcherDialog_glade.hh \
	mainWindow_glade.hh \
	mainWindow.hh \
	prefsWindow_glade.hh \
	prefsWindow.hh \
	propertiesDialog_glade.hh \
	propertiesDialog.hh \
	queryDialog_glade.hh \
	queryDialog.hh \
	statisticsDialog_glade.hh \
	statisticsDialog.hh \
	DaemonState.h \
	EnginesTree.h \
	IndexPage.h \
	ModelColumns.h \
	Notebook.h \
	OnDiskHandler.h \
	PinotSettings.h \
	PinotUtils.h \
	ResultsTree.h \
	ServerThreads.h \
	UniqueApplication.h \
	WorkerThreads.h

pinot_SOURCES = \
	pinot.cc \
	importDialog_glade.cc \
	importDialog.cc \
	indexDialog_glade.cc \
	indexDialog.cc \
	launcherDialog.cc \
	launcherDialog_glade.cc \
	mainWindow_glade.cc \
	mainWindow.cc \
	prefsWindow_glade.cc \
	prefsWindow.cc \
	propertiesDialog_glade.cc \
	propertiesDialog.cc \
	queryDialog_glade.cc \
	queryDialog.cc \
	statisticsDialog_glade.cc \
	statisticsDialog.cc \
	EnginesTree.cpp \
	IndexPage.cpp \
	ModelColumns.cpp \
	Notebook.cpp \
	PinotSettings.cpp \
	PinotUtils.cpp \
	ResultsTree.cpp \
	WorkerThreads.cpp

pinot_dbus_daemon_SOURCES = \
	pinot-dbus-daemon.cc \
	DaemonState.cpp \
	OnDiskHandler.cpp \
	PinotSettings.cpp \
	ServerThreads.cpp \
	UniqueApplication.cpp \
	WorkerThreads.cpp

bin_PROGRAMS = pinot pinot-dbus-daemon

AM_CXXFLAGS = \
	@VISIB_CFLAGS@ \
	-I$(top_srcdir)/Utils \
	-I$(top_srcdir)/Tokenize \
	-I$(top_srcdir)/Tokenize/filters \
	-I$(top_srcdir)/SQL \
	-I$(top_srcdir)/Monitor \
	-I$(top_srcdir)/Collect \
	-I$(top_srcdir)/IndexSearch \
	@SQL_CFLAGS@ @HTTP_CFLAGS@ @XML_CFLAGS@ @DBUS_CFLAGS@ \
	@INDEX_CFLAGS@ @GTHREAD_CFLAGS@ @GTKMM_CFLAGS@

pinot_LDFLAGS = \
	-rdynamic

pinot_LDADD = \
	-L$(top_builddir)/Utils \
	-L$(top_builddir)/Tokenize \
	-L$(top_builddir)/SQL \
	-L$(top_builddir)/Monitor \
	-L$(top_builddir)/Collect \
	-L$(top_builddir)/IndexSearch \
	-lIndexSearch -lMonitor -lCollect -lSQL \
	-lTokenize -lUtils -lBasicUtils @LIBS@ \
	@GTKMM_LIBS@ @DBUS_LIBS@ @GTHREAD_LIBS@ @XML_LIBS@ \
	@HTTP_LIBS@ @SQL_LIBS@ @MISC_LIBS@

pinot_dbus_daemon_LDFLAGS = \
	-rdynamic

pinot_dbus_daemon_LDADD = \
	-L$(top_builddir)/Utils \
	-L$(top_builddir)/Tokenize \
	-L$(top_builddir)/SQL \
	-L$(top_builddir)/Monitor \
	-L$(top_builddir)/Collect \
	-L$(top_builddir)/IndexSearch \
	-lIndexSearch -lMonitor -lCollect -lSQL \
	-lTokenize -lUtils -lBasicUtils @LIBS@ \
	@GLIBMM_LIBS@ @DBUS_LIBS@ @GTHREAD_LIBS@ @XML_LIBS@ \
	@HTTP_LIBS@ @SQL_LIBS@ @MISC_LIBS@

