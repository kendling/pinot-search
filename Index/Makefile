
ROOT_DIR = ..
include ${ROOT_DIR}/variables.mk

IDX_SRCS = Summarizer.cpp LanguageDetector.cpp XapianIndex.cpp
IDX_OBJS := $(patsubst %.cpp,${OBJ_DIR}/%.o,${IDX_SRCS})
IDX_TEST = ${BIN_DIR}/indextest

targets : dirs ${IDX_LIB} ${IDX_TEST} ${DISCKCAT_APP}

clean :
	@rm -f ${OBJ_DIR}/* ${IDX_LIB} ${IDX_TEST} ${DISCKCAT_APP}

# Index tester

${IDX_TEST} : ${OBJ_DIR}/indextest.o ${IDX_LIB} ${SQL_LIB} ${TOKENIZE_LIB} ${UTILS_LIB}
	@echo Building ${IDX_TEST}
	${LINK} -o $@ ${OBJ_DIR}/indextest.o ${IDX_LIB} ${SQL_LIB} ${TOKENIZE_LIB} ${UTILS_LIB} ${LIBS}

# Library

${IDX_LIB} : ${IDX_OBJS}
	@echo Building ${IDX_LIB}
	${AR} cr ${IDX_LIB} ${IDX_OBJS}
