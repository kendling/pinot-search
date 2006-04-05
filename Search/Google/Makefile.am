# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	GAPIGoogleSearchBindingProxy.h \
	GAPIH.h \
	GAPIStub.h \
	GoogleAPIEngine.h \
	GoogleSearch.h

noinst_LTLIBRARIES = libSearchGoogle.la

libSearchGoogle_la_SOURCES = \
	GAPIClientLib.cpp \
	GoogleAPIEngine.cpp

GoogleSearch.h :
	wsdl2c -n gapi -o GoogleSearch.h googleapi/GoogleSearch.wsdl

GAPIClientLib.cpp : GoogleSearch.h
	soapcpp2 -n -pGAPI -I /usr/include/gsoap/import GoogleSearch.h

AM_CXXFLAGS = -I../../Utils -I../../Tokenize -I../../Collect -I.. \
	@HTTP_CFLAGS@ @XML_CFLAGS@ @SOAP_CFLAGS@ @INDEX_CFLAGS@ @MIME_CFLAGS@

