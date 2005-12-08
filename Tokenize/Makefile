
ROOT_DIR = ..

include ${ROOT_DIR}/variables.mk

TOKENIZE_SRCS = Tokenizer.cpp HtmlTokenizer.cpp UnknownTypeTokenizer.cpp \
	TokenizerFactory.cpp
TOKENIZE_OBJS := $(patsubst %.cpp,${OBJ_DIR}/%.o,${TOKENIZE_SRCS})

TOKENIZER_TEST = ${BIN_DIR}/tokenizertest
PDF_TOKENIZER_DL = ${LIB_DIR}/pdf_tokenizer.so
WORD_TOKENIZER_DL = ${LIB_DIR}/word_tokenizer.so

targets : dirs ${TOKENIZE_LIB} ${TOKENIZER_TEST} \
	${PDF_TOKENIZER_DL} ${WORD_TOKENIZER_DL}

clean :
	@rm -f ${OBJ_DIR}/* ${TOKENIZE_LIB} ${TOKENIZER_TEST} \
	${PDF_TOKENIZER_DL} ${WORD_TOKENIZER_DL}

# Tokenize tester

${TOKENIZER_TEST} : ${OBJ_DIR}/tokenizertest.o ${TOKENIZE_LIB}
	@echo Building ${TOKENIZER_TEST}
	${LINK} -o ${TOKENIZER_TEST} ${OBJ_DIR}/tokenizertest.o ${TOKENIZE_LIB} ${UTILS_LIB} ${LIBS}

# Library

${TOKENIZE_LIB} : ${TOKENIZE_OBJS}
	@echo Building ${TOKENIZE_LIB}
	${AR} cr ${TOKENIZE_LIB} ${TOKENIZE_OBJS}

# Tokenizer libraries

${PDF_TOKENIZER_DL} : ${OBJ_DIR}/PdfTokenizer.o
	@echo Building ${PDF_TOKENIZER_DL}
	${LINK} -shared -o ${PDF_TOKENIZER_DL} ${OBJ_DIR}/PdfTokenizer.o ${TOKENIZE_LIB} ${UTILS_LIB} ${LIBS} -ldl

${WORD_TOKENIZER_DL} : ${OBJ_DIR}/WordTokenizer.o
	@echo Building ${WORD_TOKENIZER_DL}
	${LINK} -shared -o ${WORD_TOKENIZER_DL} ${OBJ_DIR}/WordTokenizer.o ${TOKENIZE_LIB} ${UTILS_LIB} ${LIBS} -ldl

