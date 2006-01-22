
ROOT_DIR = ..

include ${ROOT_DIR}/variables.mk

UTILS_SRCS = DocumentInfo.cpp Document.cpp HtmlDocument.cpp \
	IndexedDocument.cpp Languages.cpp MIMEScanner.cpp MboxParser.cpp \
	Result.cpp StringManip.cpp TimeConverter.cpp Timer.cpp Url.cpp \
	XapianDatabase.cpp XapianDatabaseFactory.cpp
UTILS_OBJS := $(patsubst %.cpp,${OBJ_DIR}/%.o,${UTILS_SRCS})

targets : dirs ${UTILS_LIB}

clean :
	@rm -f ${OBJ_DIR}/* ${UTILS_LIB}

# Library

${UTILS_LIB} : ${UTILS_OBJS}
	@echo Building ${UTILS_LIB}
	${AR} cr ${UTILS_LIB} ${UTILS_OBJS}

