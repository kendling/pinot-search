# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	HtmlTokenizer.h \
	PdfTokenizer.h \
	RtfTokenizer.h \
	Tokenizer.h \
	TokenizerFactory.h \
	UnknownTypeTokenizer.h \
	WordTokenizer.h

noinst_LTLIBRARIES = libTokenize.la

libTokenize_la_SOURCES = \
	HtmlTokenizer.cpp \
	PdfTokenizer.cpp \
	RtfTokenizer.cpp \
	Tokenizer.cpp \
	TokenizerFactory.cpp \
	UnknownTypeTokenizer.cpp \
	WordTokenizer.cpp

AM_CXXFLAGS = -I../Utils

