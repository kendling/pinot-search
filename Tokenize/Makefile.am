# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	FilterUtils.h \
	Tokenizer.h \
	filters/ExternalFilter.h \
	filters/Filter.h \
	filters/FilterFactory.h \
	filters/GMimeMboxFilter.h \
	filters/HtmlFilter.h \
	filters/TagLibMusicFilter.h \
	filters/TextFilter.h \
	filters/XmlFilter.h

lib_LTLIBRARIES = libexternalfilter.la libmboxfilter.la libtaglibfilter.la

noinst_LTLIBRARIES = libTokenize.la

libexternalfilter_la_SOURCES = \
	filters/Filter.cc \
	filters/ExternalFilter.cc

libexternalfilter_la_LDFLAGS = -module @XML_CFLAGS@

libexternalfilter_la_LIBADD =

libmboxfilter_la_SOURCES = \
	filters/Filter.cc \
	filters/GMimeMboxFilter.cc

libmboxfilter_la_LDFLAGS = -module @GMIME_CFLAGS@

libmboxfilter_la_LIBADD =

libtaglibfilter_la_SOURCES = \
	filters/Filter.cc \
	filters/TagLibMusicFilter.cc

libtaglibfilter_la_LDFLAGS = -module @TAGLIB_LIBS@

libtaglibfilter_la_LIBADD =

libTokenize_la_SOURCES = \
	FilterUtils.cpp \
	Tokenizer.cpp \
	filters/Filter.cc \
	filters/FilterFactory.cc \
	filters/HtmlFilter.cc \
	filters/TextFilter.cc \
	filters/XmlFilter.cc

AM_CXXFLAGS = -fPIC -I$(srcdir)/../Utils -Ifilters \
	@GMIME_CFLAGS@ @XML_CFLAGS@ @TAGLIB_CFLAGS@ -D_DYNAMIC_DIJON_FILTERS \
	-D_DIJON_EXTERNALFILTER_CONFFILE=\"$(sysconfdir)/pinot/external-filters.xml\"

