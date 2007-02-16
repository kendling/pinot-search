# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	FilterUtils.h \
	Tokenizer.h \
	filters/ExternalFilter.h \
	filters/HtmlFilter.h \
	filters/GMimeMboxFilter.h \
	filters/TagLibMusicFilter.h \
	filters/TextFilter.h \
	filters/Filter.h \
	filters/FilterFactory.h \
	filters/GMimeMboxFilter.h \
	filters/XmlFilter.h

lib_LTLIBRARIES = libexternalfilter.la libmboxfilter.la libtaglibfilter.la

noinst_LTLIBRARIES = libTokenize.la

libexternalfilter_la_SOURCES = \
	filters/ExternalFilter.cc

libexternalfilter_la_LDFLAGS = -module @XML_CFLAGS@

libexternalfilter_la_LIBADD = -L../Utils -lBasicUtils

libmboxfilter_la_SOURCES = \
	filters/GMimeMboxFilter.cc

libmboxfilter_la_LDFLAGS = -module @GMIME_CFLAGS@

libmboxfilter_la_LIBADD = -L../Utils -lBasicUtils

libtaglibfilter_la_SOURCES = \
	filters/TagLibMusicFilter.cc

libtaglibfilter_la_LDFLAGS = -module @TAGLIB_LIBS@

libtaglibfilter_la_LIBADD = -L../Utils -lBasicUtils

libTokenize_la_SOURCES = \
	FilterUtils.cpp \
	Tokenizer.cpp \
	filters/FilterFactory.cc \
	filters/HtmlFilter.cc \
	filters/TextFilter.cc \
	filters/XmlFilter.cc

AM_CXXFLAGS = -fPIC -I$(srcdir)/../Utils -Ifilters \
	@GMIME_CFLAGS@ @XML_CFLAGS@ @TAGLIB_CFLAGS@ -D_DYNAMIC_DIJON_FILTERS \
	-D_DIJON_EXTERNALFILTER_CONFFILE=\"$(sysconfdir)/pinot/external-filters.xml\"

