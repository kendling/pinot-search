# Process this file with automake to produce Makefile.in

noinst_HEADERS = \
	GAPI.nsmap \
	GAPIGoogleSearchBindingProxy.h \
	GAPIH.h \
	GAPIStub.h \
	GoogleAPIEngine.h \
	GoogleSearch.h

if HAVE_SOAP
noinst_LTLIBRARIES = libSearchGoogle.la

libSearchGoogle_la_SOURCES = \
	GAPIC.cpp \
	GAPIClient.cpp \
	GAPIClientLib.cpp \
	GoogleAPIEngine.cpp

GoogleSearch.h :
	wsdl2h -n gapi -o GoogleSearch.h googleapi/GoogleSearch.wsdl

GAPIClientLib.cpp : GoogleSearch.h
	soapcpp2 -n -pGAPI -I/usr/include/gsoap/import -I/usr/share/gsoap/import GoogleSearch.h

libSearchGoogle_la_CXXFLAGS = \
	-I$(top_srcdir)/Utils \
	-I$(top_srcdir)/Tokenize \
	-I$(top_srcdir)/Tokenize/filters \
	-I$(top_srcdir)/Collect \
	-I$(top_srcdir)/IndexSearch \
	@HTTP_CFLAGS@ @XML_CFLAGS@ @SOAP_CFLAGS@ \
	@INDEX_CFLAGS@
endif

