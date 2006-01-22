
ROOT_DIR = ..
include ${ROOT_DIR}/variables.mk

SE_SRCS = OpenSearchParser.cpp PluginWebEngine.cpp QueryProperties.cpp \
	SherlockParser.cpp SearchEngineFactory.cpp SearchEngineInterface.cpp \
	SearchPluginProperties.cpp WebEngine.cpp XapianEngine.cpp
SE_OBJS := $(patsubst %.cpp,${OBJ_DIR}/%.o,${SE_SRCS})
ifeq (${NEEDS_SOAP},yes)
SOAPENV_OBJS = ${OBJ_DIR}/stdsoap.o ${OBJ_DIR}/SOAPEnvNS.o ${OBJ_DIR}/SOAPEnvC.o
else
SOAPENV_OBJS = 
endif
PLUGIN_TEST = ${BIN_DIR}/plugintest
SE_TEST = ${BIN_DIR}/senginetest

targets : dirs ${SE_LIB} ${SOAPENV_LIB} ${PLUGIN_TEST} ${SE_TEST}

clean :
	@rm -f ${OBJ_DIR}/* ${SE_LIB} ${SOAPENV_LIB} ${PLUGIN_TEST} ${SE_TEST}

# SOAP environment

${OBJ_DIR}/stdsoap.o : ${GSOAP_HOME}/stdsoap2.cpp
	${CXX} -o ${OBJ_DIR}/stdsoap.o -c ${GSOAP_HOME}/stdsoap2.cpp ${CXXFLAGS}

${OBJ_DIR}/SOAPEnvNS.o : SOAPEnvNS.cpp
	${CXX} -o ${OBJ_DIR}/SOAPEnvNS.o -c SOAPEnvNS.cpp ${CXXFLAGS}

SOAPEnv.h :
	# Create an empty file, we just want to get global functions
	@touch SOAPEnv.h

SOAPEnvC.cpp : SOAPEnv.h
	${SOAPC} -pSOAPEnv SOAPEnv.h

${OBJ_DIR}/SOAPEnvC.o : SOAPEnvC.cpp
	${CXX} -o ${OBJ_DIR}/SOAPEnvC.o -c SOAPEnvC.cpp ${CXXFLAGS}

# Parser tester

${PLUGIN_TEST} : ${OBJ_DIR}/plugintest.o ${SE_LIBS} ${DL_LIB} ${TOKENIZE_LIB} ${UTILS_LIB}
	@echo Building ${PLUGIN_TEST}
	${LINK} -o $@ ${OBJ_DIR}/plugintest.o ${SE_LIBS} ${DL_LIB} ${TOKENIZE_LIB} ${UTILS_LIB} ${LIBS}

# SearchEngine tester

${SE_TEST} : ${OBJ_DIR}/senginetest.o ${SE_LIBS} ${DL_LIB} ${TOKENIZE_LIB} ${UTILS_LIB}
	@echo Building ${SE_TEST}
	${LINK} -o $@ ${OBJ_DIR}/senginetest.o ${SE_LIBS} ${DL_LIB} ${TOKENIZE_LIB} ${UTILS_LIB} ${LIBS}
	
# Libraries

${SE_LIB} : ${SE_OBJS}
	@echo Building ${SE_LIB}
	${AR} cr ${SE_LIB} ${SE_OBJS}

${SOAPENV_LIB} : ${SOAPENV_OBJS}
ifeq (${NEEDS_SOAP},yes)
	@echo Building ${SOAPENV_LIB}
	${AR} cr ${SOAPENV_LIB} ${SOAPENV_OBJS}
else
	@echo Not building ${SOAPENV_LIB}
endif
