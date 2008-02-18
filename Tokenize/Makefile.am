# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	FilterUtils.h \
	TextConverter.h \
	Tokenizer.h \
	$(top_srcdir)/Tokenize/filters/ExternalFilter.h \
	$(top_srcdir)/Tokenize/filters/Filter.h \
	$(top_srcdir)/Tokenize/filters/FilterFactory.h \
	$(top_srcdir)/Tokenize/filters/GMimeMboxFilter.h \
	$(top_srcdir)/Tokenize/filters/HtmlFilter.h \
	$(top_srcdir)/Tokenize/filters/TagLibMusicFilter.h \
	$(top_srcdir)/Tokenize/filters/TextFilter.h \
	$(top_srcdir)/Tokenize/filters/XmlFilter.h

lib_LTLIBRARIES = libexternalfilter.la libmboxfilter.la libtaglibfilter.la

noinst_LTLIBRARIES = libTokenize.la

libexternalfilter_la_SOURCES = \
	$(top_srcdir)/Tokenize/filters/Filter.cc \
	$(top_srcdir)/Tokenize/filters/ExternalFilter.cc

libexternalfilter_la_LDFLAGS = -module @XML_LIBS@

libexternalfilter_la_LIBADD =

libmboxfilter_la_SOURCES = \
	$(top_srcdir)/Tokenize/filters/Filter.cc \
	$(top_srcdir)/Tokenize/filters/GMimeMboxFilter.cc

libmboxfilter_la_LDFLAGS = -module @GMIME_LIBS@

libmboxfilter_la_LIBADD =

libtaglibfilter_la_SOURCES = \
	$(top_srcdir)/Tokenize/filters/Filter.cc \
	$(top_srcdir)/Tokenize/filters/TagLibMusicFilter.cc

libtaglibfilter_la_LDFLAGS = -module @TAGLIB_LIBS@

libtaglibfilter_la_LIBADD =

libTokenize_la_SOURCES = \
	FilterUtils.cpp \
	TextConverter.cpp \
	Tokenizer.cpp \
	$(top_srcdir)/Tokenize/filters/Filter.cc \
	$(top_srcdir)/Tokenize/filters/FilterFactory.cc \
	$(top_srcdir)/Tokenize/filters/HtmlFilter.cc \
	$(top_srcdir)/Tokenize/filters/TextFilter.cc \
	$(top_srcdir)/Tokenize/filters/XmlFilter.cc

AM_CXXFLAGS = \
	-fPIC \
	-I$(top_srcdir)/Utils -Ifilters \
	@GMIME_CFLAGS@ @XML_CFLAGS@ @TAGLIB_CFLAGS@ -D_DYNAMIC_DIJON_FILTERS \
	-D_DIJON_EXTERNALFILTER_CONFFILE=\"$(sysconfdir)/pinot/external-filters.xml\"

