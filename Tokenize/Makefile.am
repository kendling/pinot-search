# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	HtmlTokenizer.h \
	PdfTokenizer.h \
	RtfTokenizer.h \
	Tokenizer.h \
	TokenizerFactory.h \
	UnknownTypeTokenizer.h \
	WordTokenizer.h

lib_LTLIBRARIES = libpdftokenizer.la librtftokenizer.la libwordtokenizer.la

noinst_LTLIBRARIES = libTokenize.la

libpdftokenizer_la_SOURCES = \
	PdfTokenizer.cpp

libpdftokenizer_la_LDFLAGS = -L../Utils 

libpdftokenizer_la_LIBADD = libTokenize.la ../Utils/libUtils.la

librtftokenizer_la_SOURCES = \
	RtfTokenizer.cpp

librtftokenizer_la_LDFLAGS = -L../Utils

librtftokenizer_la_LIBADD = libTokenize.la ../Utils/libUtils.la

libwordtokenizer_la_SOURCES = \
	WordTokenizer.cpp

libwordtokenizer_la_LDFLAGS = -L../Utils

libwordtokenizer_la_LIBADD = libTokenize.la ../Utils/libUtils.la

libTokenize_la_SOURCES = \
	HtmlTokenizer.cpp \
	Tokenizer.cpp \
	TokenizerFactory.cpp \
	UnknownTypeTokenizer.cpp

AM_CXXFLAGS = -I../Utils

