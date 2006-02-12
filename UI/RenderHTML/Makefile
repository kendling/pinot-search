
ROOT_DIR = ../..
include ${ROOT_DIR}/variables.mk

CXXFLAGS += -DHAVE_CONFIG_H -I. -I.. ${GTKMOZ_CXXFLAGS} 

LIBS += ${MOZILLA_XPCOM_LIBS} ${GTKMOZ_LIBS}

HTML_SRCS = MozillaRenderer.cpp
HTML_OBJS := $(patsubst %.cpp,${OBJ_DIR}/%.o,${HTML_SRCS})

targets : dirs ${HTML_LIB}

clean :
	@rm -f ${OBJ_DIR}/* ${HTML_LIB}

# Library
 
${HTML_LIB} : ${HTML_OBJS}
	@echo Building ${HTML_LIB}
	${AR} cr ${HTML_LIB} ${HTML_OBJS}
