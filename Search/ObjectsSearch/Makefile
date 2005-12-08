
ROOT_DIR = ../..
include ${ROOT_DIR}/variables.mk

OSAPI_SRCS = OSAPIClientLib.cpp ObjectsSearchAPIEngine.cpp
OSAPI_OBJS := $(patsubst %.cpp,${OBJ_DIR}/%.o,${OSAPI_SRCS})

targets : dirs ${OSAPI_LIB}

clean : clean-stubs
	@rm -f ${OBJ_DIR}/* ${OSAPI_LIB}

clean-stubs:
	@rm -f OSAPI* ${OBJ_DIR}/OSAPI* *.xml

# ObjectsSearch API stuff

ObjectsSearch.h : ObjectsSearch.wsdl
	${WSDLC} -n osapi -o ObjectsSearch.h ObjectsSearch.wsdl

OSAPIClientLib.cpp : ObjectsSearch.h
	${SOAPC} -n -pOSAPI ObjectsSearch.h

# Library

${OSAPI_LIB} : ${OSAPI_OBJS}
	@echo Building ${OSAPI_LIB}
	${AR} cr ${OSAPI_LIB} ${OSAPI_OBJS}
