
ROOT_DIR = ..
include ${ROOT_DIR}/variables.mk

DL_SRCS = DownloaderInterface.cpp NeonDownloader.cpp XapianCollector.cpp \
        FileCollector.cpp MboxCollector.cpp DownloaderFactory.cpp
DL_OBJS := $(patsubst %.cpp,${OBJ_DIR}/%.o,${DL_SRCS})
DL_TEST = ${BIN_DIR}/dloadtest

targets : dirs ${DL_LIB} ${DL_TEST}

clean :
	@rm -f ${OBJ_DIR}/* ${DL_LIB} ${DL_TEST}

# Downloader tester

${DL_TEST} : ${OBJ_DIR}/dloadtest.o ${DL_LIB} ${TOKENIZE_LIB} ${UTILS_LIB}
	@echo Building ${DL_TEST}
	${LINK} -o $@ ${OBJ_DIR}/dloadtest.o ${DL_LIB} ${TOKENIZE_LIB} ${UTILS_LIB} ${LIBS}

# Library

${DL_LIB} : ${DL_OBJS}
	@echo Building ${DL_LIB}
	${AR} cr ${DL_LIB} ${DL_OBJS}
