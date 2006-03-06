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
	PdfTokenizer.cpp \
	HtmlTokenizer.cpp \
	Tokenizer.cpp

libpdftokenizer_la_LIBADD = -L../Utils -lUtils

librtftokenizer_la_SOURCES = \
	RtfTokenizer.cpp \
	HtmlTokenizer.cpp \
	Tokenizer.cpp

librtftokenizer_la_LIBADD = -L../Utils -lUtils

libwordtokenizer_la_SOURCES = \
	WordTokenizer.cpp \
	Tokenizer.cpp

libwordtokenizer_la_LIBADD = -L../Utils -lUtils

libTokenize_la_SOURCES = \
	HtmlTokenizer.cpp \
	Tokenizer.cpp \
	TokenizerFactory.cpp \
	UnknownTypeTokenizer.cpp

AM_CXXFLAGS = -I../Utils

