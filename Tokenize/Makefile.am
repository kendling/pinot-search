# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	HtmlTokenizer.h \
	OpenDocumentTokenizer.h \
	PdfTokenizer.h \
	RtfTokenizer.h \
	TagLibTokenizer.h \
	Tokenizer.h \
	TokenizerFactory.h \
	UnknownTypeTokenizer.h \
	WordTokenizer.h \
	XmlTokenizer.h

lib_LTLIBRARIES = libopendocumenttokenizer.la libpdftokenizer.la librtftokenizer.la \
	libwordtokenizer.la libtaglibtokenizer.la

noinst_LTLIBRARIES = libTokenize.la

libopendocumenttokenizer_la_SOURCES = \
	OpenDocumentTokenizer.cpp \
	XmlTokenizer.cpp \
	Tokenizer.cpp

libopendocumenttokenizer_la_LDFLAGS = -module

libopendocumenttokenizer_la_LIBADD = -L../Utils -lBasicUtils

libpdftokenizer_la_SOURCES = \
	PdfTokenizer.cpp \
	HtmlTokenizer.cpp \
	Tokenizer.cpp

libpdftokenizer_la_LDFLAGS = -module

libpdftokenizer_la_LIBADD = -L../Utils -lBasicUtils

librtftokenizer_la_SOURCES = \
	RtfTokenizer.cpp \
	HtmlTokenizer.cpp \
	Tokenizer.cpp

librtftokenizer_la_LDFLAGS = -module

librtftokenizer_la_LIBADD = -L../Utils -lBasicUtils

libwordtokenizer_la_SOURCES = \
	WordTokenizer.cpp \
	Tokenizer.cpp

libwordtokenizer_la_LDFLAGS = -module

libwordtokenizer_la_LIBADD = -L../Utils -lBasicUtils

libtaglibtokenizer_la_SOURCES = \
	TagLibTokenizer.cpp \
	Tokenizer.cpp

libtaglibtokenizer_la_LDFLAGS = -module @TAGLIB_LIBS@

libtaglibtokenizer_la_LIBADD = -L../Utils -lBasicUtils

libTokenize_la_SOURCES = \
	HtmlTokenizer.cpp \
	Tokenizer.cpp \
	TokenizerFactory.cpp \
	UnknownTypeTokenizer.cpp \
	XmlTokenizer.cpp

AM_CXXFLAGS = -fPIC -I$(srcdir)/../Utils @XML_CFLAGS@ @TAGLIB_CFLAGS@

