CXX := @g++
LINK := @libtool --mode=link g++
AR := @ar
WSDLC := @${GSOAP_HOME}/wsdl2h
SOAPC := @${GSOAP_HOME}/soapcpp2 -I${GSOAP_HOME}

HW_NAME := $(shell uname -i)
OBJ_DIR := obj-${HW_NAME}
LIB_DIR := ${ROOT_DIR}/lib-${HW_NAME}
BIN_DIR := ${ROOT_DIR}/bin-${HW_NAME}

# Libraries

UTILS_LIB := ${LIB_DIR}/libutils.a
TOKENIZE_LIB := ${LIB_DIR}/libtokenize.a
SQL_LIB := ${LIB_DIR}/libsql.a
DL_LIB := ${LIB_DIR}/libcollect.a
GAPI_LIB := ${LIB_DIR}/libgoogleapi.a
OSAPI_LIB := ${LIB_DIR}/libosapi.a
SE_LIB := ${LIB_DIR}/libsearch.a
SE_LIBS := ${SE_LIB}
SOAPENV_LIB := ${LIB_DIR}/libsoapenv.a
IDX_LIB := ${LIB_DIR}/libindex.a
HTML_LIB := ${LIB_DIR}/libhtml.a

# Flags

PROJECT_CXXFLAGS := -I${ROOT_DIR}/Utils -I${ROOT_DIR}/Tokenize -I${ROOT_DIR}/SQL \
	-I${ROOT_DIR}/Collect -I${ROOT_DIR}/Search -I${ROOT_DIR}/Index \
	-I${ROOT_DIR}/UI/RenderHTML

# SOAP
NEEDS_SOAP := no
# Google SOAP API is optional
ifeq ($(HAS_GOOGLEAPI),yes)
GOOGLEAPI_CXXFLAGS := -I${GSOAP_HOME} -I${ROOT_DIR}/Search/Google -DHAS_GOOGLEAPI
SE_LIBS += ${GAPI_LIB}
NEEDS_SOAP := yes
else
GOOGLEAPI_CXXFLAGS :=
endif
# ObjectsSearch SOAP API is also optional
ifeq ($(HAS_OSAPI),yes)
OSAPI_CXXFLAGS := -I${GSOAP_HOME} -I${ROOT_DIR}/Search/ObjectsSearch -DHAS_OSAPI
SE_LIBS += ${OSAPI_LIB}
NEEDS_SOAP := yes
else
OSAPI_CXXFLAGS :=
endif
ifeq ($(NEEDS_SOAP),yes)
SE_LIBS += ${SOAPENV_LIB}
endif

# NEON
NEON_CXXFLAGS = $(shell neon-config --cflags)
NEON_LIBS = $(shell neon-config --libs)
# OTS
OTS_CXXFLAGS = $(shell /usr/bin/pkg-config --cflags libots-1)
OTS_LIBS = $(shell /usr/bin/pkg-config --libs libots-1)
# GMime
GMIME_CXXFLAGS = $(shell /usr/bin/pkg-config --cflags gmime-2.0)
GMIME_LIBS = $(shell /usr/bin/pkg-config --libs gmime-2.0)
# Libtextcat
TEXTCAT_CXXFLAGS =
TEXTCAT_LIBS = -ltextcat
# Xapian
XAPIAN_CXXFLAGS = $(shell xapian-config --cxxflags)
XAPIAN_LIBS = $(shell xapian-config --libs)
# SQLite
SQLITE_CXXFLAGS = $(shell /usr/bin/pkg-config --cflags sqlite3)
SQLITE_LIBS = $(shell /usr/bin/pkg-config --libs sqlite3)
# LibXML 2.0
LIBXML_CXXFLAGS = $(shell /usr/bin/pkg-config --cflags libxml++-2.6)
LIBXML_LIBS = $(shell /usr/bin/pkg-config --libs libxml++-2.6)
# Mozilla
MOZILLA_LIB_DIR = $(shell dirname `find /usr/lib*/mozilla* -name libgtkembedmoz.so | head -1`)
ifeq ($(MOZILLA_LIB_DIR),)
MOZILLA_LIB_DIR = /usr/lib/mozilla
endif
MOZILLA_INC_DIR = $(shell dirname `find /usr/include/mozilla* -name mozilla-config.h | head -1`)
ifeq ($(MOZILLA_INC_DIR),)
MOZILLA_INC_DIR = /usr/include/mozilla
endif
MOZILLA_XPCOM_CXXFLAGS = $(shell /usr/bin/pkg-config --cflags mozilla-xpcom)
MOZILLA_XPCOM_LIBS = -Xlinker -rpath -Xlinker ${MOZILLA_LIB_DIR} $(shell /usr/bin/pkg-config --libs mozilla-xpcom)
GTKMOZ_CXXFLAGS = $(shell /usr/bin/pkg-config --cflags mozilla-gtkmozembed gtk+-2.0)
GTKMOZ_LIBS = -Xlinker -rpath -Xlinker ${MOZILLA_LIB_DIR} $(shell /usr/bin/pkg-config --libs mozilla-gtkmozembed gtk+-2.0)
# GTKmm 2.0
GTKMM_CXXFLAGS = $(shell /usr/bin/pkg-config --cflags gtkmm-2.4)
GTKMM_LIBS = $(shell /usr/bin/pkg-config --libs gtkmm-2.4)

CXXFLAGS = -DENABLE_NLS ${PROJECT_CXXFLAGS} ${GOOGLEAPI_CXXFLAGS} ${OSAPI_CXXFLAGS} \
	${NEON_CXXFLAGS} ${OTS_CXXFLAGS} ${GMIME_CXXFLAGS} ${TEXTCAT_CXXFLAGS} \
	${LIBXML_CXXFLAGS} ${XAPIAN_CXXFLAGS} ${SQLITE_CXXFLAGS}
ifeq ($(DEBUG),yes)
CXXFLAGS += -g -DDEBUG
endif
LIBS := -lmagic -lpthread -lcrypt ${NEON_LIBS} ${OTS_LIBS} ${GMIME_LIBS} ${TEXTCAT_LIBS} \
	${LIBXML_LIBS} ${XAPIAN_LIBS} ${SQLITE_LIBS} ${MOZILLA_XPCOM_LIBS}

# Common targets

all : targets

dirs :
	@mkdir -p ${OBJ_DIR} ${LIB_DIR} ${BIN_DIR}

${OBJ_DIR}/%.o : %.cpp
	${CXX} -c $< -o $@ ${CXXFLAGS}

${OBJ_DIR}/%.o : %.cc
	${CXX} -c $< -o $@ ${CXXFLAGS}

