
ROOT_DIR = ../..
include ${ROOT_DIR}/variables.mk

GAPI_SRCS = GAPIClientLib.cpp GoogleAPIEngine.cpp
GAPI_OBJS := $(patsubst %.cpp,${OBJ_DIR}/%.o,${GAPI_SRCS})

targets : dirs ${GAPI_LIB}

clean : clean-stubs
	@rm -f ${OBJ_DIR}/* ${GAPI_LIB}

clean-stubs:
	@rm -f GAPI* ${OBJ_DIR}/GAPI* *.xml

# Google API stuff

GoogleSearch.h : googleapi/GoogleSearch.wsdl
	${WSDLC} -n gapi -o GoogleSearch.h googleapi/GoogleSearch.wsdl

GAPIClientLib.cpp : GoogleSearch.h
	${SOAPC} -n -pGAPI GoogleSearch.h

# Library

${GAPI_LIB} : ${GAPI_OBJS}
	@echo Building ${GAPI_LIB}
	${AR} cr ${GAPI_LIB} ${GAPI_OBJS}
