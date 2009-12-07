find_package(Qt4 REQUIRED)
set(QT_USE_QTMAIN 1)
include(${QT_USE_FILE})

include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})

# Platform specific settings
if(WIN32)
  # creates winmain binary
  set(GUI_TYPE WIN32)
endif(WIN32)

if(APPLE)
  set(GUI_TYPE MACOSX_BUNDLE)
endif(APPLE)

# main executable
add_executable(divine.ide ${GUI_TYPE} main.cpp)
target_link_libraries(divine.ide base_shared ${QT_LIBRARIES})

set(BIN_PATH ${CMAKE_INSTALL_PREFIX}/${GUI_BIN_DEST})
set(LIB_PATH ${CMAKE_INSTALL_PREFIX}/${GUI_LIB_DEST})
set(LIBEXEC_PATH ${CMAKE_INSTALL_PREFIX}/${GUI_LIBEXEC_DEST})
set(PLUGIN_PATH ${CMAKE_INSTALL_PREFIX}/${GUI_PLUGIN_DEST})

install(TARGETS divine.ide DESTINATION ${GUI_LIBEXEC_DEST} COMPONENT gui_tools)
install(CODE "file(WRITE ${BIN_PATH}/divine.ide \"#!/bin/sh\nexport LD_LIBRARY_PATH='${LIB_PATH}':$LD_LIBRARY_PATH\nexport DIVINE_GUI_PLUGIN_PATH='${PLUGIN_PATH}'\n${LIBEXEC_PATH}/divine.ide\n\") ")
install(CODE "execute_process(COMMAND chmod a+x ${BIN_PATH}/divine.ide)")

# install required Qt components
if(WIN32)
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    install(FILES ${QT_BINARY_DIR}/QtCore4.dll ${QT_BINARY_DIR}/QtGui4.dll DESTINATION ./ COMPONENT qt)
  else(CMAKE_BUILD_TYPE STREQUAL "Release")
    install(FILES ${QT_BINARY_DIR}/QtCore4d.dll ${QT_BINARY_DIR}/QtGui4d.dll DESTINATION ./ COMPONENT qt)
  endif(CMAKE_BUILD_TYPE STREQUAL "Release")
endif(WIN32)
