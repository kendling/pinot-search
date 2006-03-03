# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	IndexInterface.h \
	LanguageDetector.h \
	XapianIndex.h

noinst_LTLIBRARIES = libIndex.la

libIndex_la_SOURCES = \
	LanguageDetector.cpp \
	XapianIndex.cpp

AM_CXXFLAGS = -I../Utils -I../Tokenize @INDEX_CFLAGS@ 

