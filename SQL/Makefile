
ROOT_DIR = ..
include ${ROOT_DIR}/variables.mk

SQL_SRCS = SQLiteBase.cpp ActionHistory.cpp IndexHistory.cpp \
	QueryHistory.cpp ViewHistory.cpp LabelManager.cpp
SQL_OBJS := $(patsubst %.cpp,${OBJ_DIR}/%.o,${SQL_SRCS})
SQL_TEST = ${BIN_DIR}/historytest

targets : dirs ${SQL_LIB} ${SQL_TEST}

clean :
	@rm -f ${OBJ_DIR}/* ${SQL_LIB} ${SQL_TEST}

# SQL tester

${SQL_TEST} : ${OBJ_DIR}/historytest.o ${SQL_LIB} ${UTILS_LIB}
	@echo Building ${SQL_TEST}
	${LINK} -o $@ ${OBJ_DIR}/historytest.o ${SQL_LIB} ${UTILS_LIB} ${LIBS}

# Library

${SQL_LIB} : ${SQL_OBJS}
	@echo Building ${SQL_LIB}
	${AR} cr ${SQL_LIB} ${SQL_OBJS}
