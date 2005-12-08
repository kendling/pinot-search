
ROOT_DIR = ..

include ${ROOT_DIR}/variables.mk

UTILS_SRCS = DocumentInfo.cpp Document.cpp HtmlDocument.cpp \
	IndexedDocument.cpp Languages.cpp MIMEScanner.cpp MboxParser.cpp \
	PluginParser.cpp Result.cpp StringManip.cpp TimeConverter.cpp \
	Timer.cpp Url.cpp XapianDatabase.cpp XapianDatabaseFactory.cpp
UTILS_OBJS := $(patsubst %.cpp,${OBJ_DIR}/%.o,${UTILS_SRCS})

PLUGIN_TEST = ${BIN_DIR}/plugintest

targets : dirs ${UTILS_LIB} ${TOKENIZER_TEST} ${PLUGIN_TEST} \
	${PDF_TOKENIZER_DL} ${WORD_TOKENIZER_DL}

clean :
	@rm -f ${OBJ_DIR}/* ${UTILS_LIB} ${TOKENIZER_TEST} \
	${PLUGIN_TEST} ${PDF_TOKENIZER_DL} ${WORD_TOKENIZER_DL}

# Utils tester

${PLUGIN_TEST} : ${OBJ_DIR}/plugintest.o ${UTILS_LIB}
	@echo Building ${PLUGIN_TEST}
	${LINK} -o ${PLUGIN_TEST} ${OBJ_DIR}/plugintest.o ${UTILS_LIB} ${LIBS}

# Library

${UTILS_LIB} : ${UTILS_OBJS}
	@echo Building ${UTILS_LIB}
	${AR} cr ${UTILS_LIB} ${UTILS_OBJS}

