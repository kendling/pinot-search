# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	GAPI.nsmap \
	GAPIGoogleSearchBindingProxy.h \
	GAPIH.h \
	GAPIStub.h \
	GoogleAPIEngine.h \
	GoogleSearch.h

noinst_LTLIBRARIES = libSearchGoogle.la

libSearchGoogle_la_SOURCES = \
	GAPIC.cpp \
	GAPIClient.cpp \
	GAPIClientLib.cpp \
	GoogleAPIEngine.cpp

GoogleSearch.h :
	wsdl2h -n gapi -o GoogleSearch.h googleapi/GoogleSearch.wsdl

GAPIClientLib.cpp : GoogleSearch.h
	soapcpp2 -n -pGAPI -I /usr/include/gsoap/import GoogleSearch.h

AM_CXXFLAGS = -I$(srcdir)/../../Utils -I$(srcdir)/../../Tokenize \
	-I$(srcdir)/../../Tokenize/filters -I../../Collect -I../../Index -I.. \
	@HTTP_CFLAGS@ @XML_CFLAGS@ @SOAP_CFLAGS@ @INDEX_CFLAGS@ @GMIME_CFLAGS@

