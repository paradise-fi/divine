find_package(Qt4 REQUIRED)
set(QT_USE_QTMAIN 1)
include(${QT_USE_FILE})

include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# Platform specific settings
if(WIN32)
  # creates winmain binary
  set(GUI_TYPE WIN32)
endif(WIN32)

if(APPLE)
  set(GUI_TYPE MACOSX_BUNDLE)
endif(APPLE)

qt4_add_resources(DIVINE_IDE_RES main.qrc)

# main executable
add_executable(divine.ide ${GUI_TYPE} main.cpp ${DIVINE_IDE_RES})
target_link_libraries(divine.ide libdivine-instances baseIDE baseTools dveModule ${QT_LIBRARIES})

set(BIN_PATH ${CMAKE_INSTALL_PREFIX}/${GUI_BIN_DEST})
set(LIB_PATH ${CMAKE_INSTALL_PREFIX}/${GUI_LIB_DEST})
set(LIBEXEC_PATH ${CMAKE_INSTALL_PREFIX}/${GUI_LIBEXEC_DEST})
set(HELP_PATH ${CMAKE_INSTALL_PREFIX}/${GUI_HELP_DEST})

install(TARGETS divine.ide DESTINATION ${GUI_LIBEXEC_DEST} COMPONENT gui_tools)
install(CODE "file(WRITE \$ENV{DESTDIR}${BIN_PATH}/divine.ide \"#!/bin/sh\nexport LD_LIBRARY_PATH='${LIB_PATH}':$LD_LIBRARY_PATH\nexport DIVINE_GUI_PLUGIN_PATH='${PLUGIN_PATH}'\nexport DIVINE_GUI_HELP_PATH='${HELP_PATH}'\n${LIBEXEC_PATH}/divine.ide\n\") ")
install(CODE "execute_process(COMMAND chmod a+x \$ENV{DESTDIR}${BIN_PATH}/divine.ide)")
