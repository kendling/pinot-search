
ROOT_DIR = ../../..
include ${ROOT_DIR}/variables.mk

CXXFLAGS += -DHAVE_CONFIG_H -I. -I.. ${LIBXML_CXXFLAGS} ${GTKMOZ_CXXFLAGS} ${GTKMM_CXXFLAGS}

LIBS += ${LIBXML_LIBS} ${GTKMOZ_LIBS} ${GTKMM_LIBS} -lgthread-2.0 -lfam

UI_GTK2_SRCS = aboutDialog.cc aboutDialog_glade.cc \
	importDialog.cc importDialog_glade.cc \
	indexDialog.cc indexDialog_glade.cc \
	prefsDialog.cc prefsDialog_glade.cc \
	propertiesDialog.cc propertiesDialog_glade.cc \
	queryDialog.cc queryDialog_glade.cc \
	mainWindow.cc mainWindow_glade.cc
UI_SRCS = pinot.cpp EnginesTree.cpp IndexPage.cpp IndexTree.cpp \
	HtmlView.cpp ModelColumns.cpp MonitorHandler.cpp Notebook.cpp \
	PinotSettings.cpp PinotUtils.cpp ResultsTree.cpp WorkerThreads.cpp
UI_GTK2_OBJS := $(patsubst %.cc,${OBJ_DIR}/%.o,${UI_GTK2_SRCS})
UI_OBJS := $(patsubst %.cpp,${OBJ_DIR}/%.o,${UI_SRCS})
UI_GTK2_BIN = ${BIN_DIR}/pinot

targets : dirs ${UI_GTK2_BIN}

clean :
	@rm -f ${OBJ_DIR}/* ${UI_GTK2_BIN}

# GTK+ user interface

${UI_GTK2_BIN} : ${UI_GTK2_OBJS} ${UI_OBJS} ${HTML_LIB} ${IDX_LIB} ${SE_LIBS} ${DL_LIB} ${SQL_LIB} ${TOKENIZE_LIB} ${UTILS_LIB}
	@echo Building ${UI_GTK2_BIN}
	${LINK} -o $@ ${UI_GTK2_OBJS} ${UI_OBJS} ${HTML_LIB} ${IDX_LIB} ${SE_LIBS} ${DL_LIB} ${SQL_LIB} ${TOKENIZE_LIB} ${UTILS_LIB} ${LIBS}
