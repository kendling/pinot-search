# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	FilterUtils.h \
	TextConverter.h \
	$(top_srcdir)/IndexSearch/cjkv/CJKVTokenizer.h \
	$(top_srcdir)/Tokenize/filters/ArchiveFilter.h \
	$(top_srcdir)/Tokenize/filters/ExifImageFilter.h \
	$(top_srcdir)/Tokenize/filters/ExternalFilter.h \
	$(top_srcdir)/Tokenize/filters/Filter.h \
	$(top_srcdir)/Tokenize/filters/FilterFactory.h \
	$(top_srcdir)/Tokenize/filters/GMimeMboxFilter.h \
	$(top_srcdir)/Tokenize/filters/HtmlFilter.h \
	$(top_srcdir)/Tokenize/filters/HtmlParser.h \
	$(top_srcdir)/Tokenize/filters/TagLibMusicFilter.h \
	$(top_srcdir)/Tokenize/filters/TextFilter.h \
	$(top_srcdir)/Tokenize/filters/XmlFilter.h

if HAVE_LIBARCHIVE
lib_LTLIBRARIES = libarchivefilter.la libexifimagefilter.la libexternalfilter.la libmboxfilter.la libtaglibfilter.la
else
lib_LTLIBRARIES = libexifimagefilter.la libexternalfilter.la libmboxfilter.la libtaglibfilter.la
endif

noinst_LTLIBRARIES = libTokenize.la

if HAVE_LIBARCHIVE
libarchivefilter_la_SOURCES = \
	$(top_srcdir)/Tokenize/filters/Filter.cc \
	$(top_srcdir)/Tokenize/filters/ArchiveFilter.cc

libarchivefilter_la_LDFLAGS = -module -avoid-version

libarchivefilter_la_LIBADD = -larchive
endif

libexifimagefilter_la_SOURCES = \
	$(top_srcdir)/Tokenize/filters/Filter.cc \
	$(top_srcdir)/Tokenize/filters/ExifImageFilter.cc

libexifimagefilter_la_LDFLAGS = -module -avoid-version

libexifimagefilter_la_LIBADD = @EXIF_LIBS@

libexternalfilter_la_SOURCES = \
	$(top_srcdir)/Tokenize/filters/Filter.cc \
	$(top_srcdir)/Tokenize/filters/ExternalFilter.cc

libexternalfilter_la_LDFLAGS = -module -avoid-version

libexternalfilter_la_LIBADD = @XML_LIBS@

libmboxfilter_la_SOURCES = \
	$(top_srcdir)/Tokenize/filters/Filter.cc \
	$(top_srcdir)/Tokenize/filters/GMimeMboxFilter.cc

libmboxfilter_la_LDFLAGS = -module -avoid-version

libmboxfilter_la_LIBADD = @GMIME_LIBS@

libtaglibfilter_la_SOURCES = \
	$(top_srcdir)/Tokenize/filters/Filter.cc \
	$(top_srcdir)/Tokenize/filters/TagLibMusicFilter.cc

libtaglibfilter_la_LDFLAGS = -module -avoid-version

libtaglibfilter_la_LIBADD = @TAGLIB_LIBS@

libTokenize_la_SOURCES = \
	FilterUtils.cpp \
	TextConverter.cpp \
	$(top_srcdir)/IndexSearch/cjkv/CJKVTokenizer.cc \
	$(top_srcdir)/Tokenize/filters/Filter.cc \
	$(top_srcdir)/Tokenize/filters/FilterFactory.cc \
	$(top_srcdir)/Tokenize/filters/HtmlFilter.cc \
	$(top_srcdir)/Tokenize/filters/HtmlParser.cc \
	$(top_srcdir)/Tokenize/filters/TextFilter.cc \
	$(top_srcdir)/Tokenize/filters/XmlFilter.cc

AM_CXXFLAGS = \
	@MISC_CFLAGS@ \
	-I$(top_srcdir)/Utils -Ifilters \
	@GMIME_CFLAGS@ @XML_CFLAGS@ @EXIF_CFLAGS@ @TAGLIB_CFLAGS@ \
	-D_DYNAMIC_DIJON_FILTERS \
	-D_DIJON_EXTERNALFILTER_CONFFILE=\"$(sysconfdir)/pinot/external-filters.xml\"

