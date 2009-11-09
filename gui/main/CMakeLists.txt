find_package(Qt4 REQUIRED)
SET(QT_USE_QTMAIN 1)
include(${QT_USE_FILE})

include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})

# Platform specific settings
IF(WIN32)
#  SET(GUI_TYPE WIN32)
ENDIF(WIN32)
IF (APPLE)
  SET(GUI_TYPE MACOSX_BUNDLE)
ENDIF (APPLE)

# main executable
add_executable(divine.ide ${GUI_TYPE} main.cpp)
target_link_libraries(divine.ide base_shared ${QT_LIBRARIES})

install(TARGETS divine.ide DESTINATION ${GUI_LIB_PATH})
install(CODE "file(WRITE ${GUI_BIN_PATH}/divine.ide \"#!/bin/sh\nexport LD_LIBRARY_PATH='${GUI_LIB_PATH}':$LD_LIBRARY_PATH\nexport DIVINE_GUI_PLUGIN_PATH='${GUI_PLUGIN_PATH}'\n${GUI_LIB_PATH}/divine.ide\n\") ")
install(CODE "execute_process(COMMAND chmod a+x ${GUI_BIN_PATH}/divine.ide)")
