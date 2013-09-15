project(gui)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
add_definitions( ${MPI_COMPILE_FLAGS} ${LLVM_COMPILE_FLAGS} )

if(WIN32)
  set(GUI_BIN_DEST ./)
  set(GUI_LIB_DEST ./)
  set(GUI_LIBEXEC_DEST ./)
  set(GUI_HELP_DEST help)
else()
  set(GUI_BIN_DEST bin)
  set(GUI_LIB_DEST lib/divine)
  set(GUI_LIBEXEC_DEST libexec/divine)
  set(GUI_HELP_DEST share/divine/help)
endif()

# if(WIN32)
#   add_definitions(-static-libgcc)
# endif(WIN32)

# Disable assertions in release code
if(NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug")
  add_definitions(-DQT_NO_DEBUG)
endif(NOT ${CMAKE_BUILD_TYPE} STREQUAL "Debug")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/baseIde)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(qtcolorpicker)

add_subdirectory(qtcolorpicker)

add_subdirectory(main)
add_subdirectory(baseIde)
add_subdirectory(baseTools)
add_subdirectory(dve)

# Install the shared Qt component
if(WIN32)
  find_path(GUI_QTCORE_DLL QtCore4.dll)
  find_path(GUI_QTGUI_DLL QtGui4.dll)

  install(FILES ${GUI_QTCORE_DLL}/QtCore4.dll DESTINATION ${GUI_BIN_DEST} COMPONENT qt)
  install(FILES ${GUI_QTGUI_DLL}/QtGui4.dll DESTINATION ${GUI_BIN_DEST} COMPONENT qt)
endif(WIN32)

if(WIN32)
  find_path(GUI_ASSISTANT_EXECUTABLE assistant.exe)
  find_path(GUI_QTHELP_DLL QtHelp4.dll)
  find_path(GUI_QTCLUCENE_DLL QtCLucene4.dll)
  find_path(GUI_QTNETWORK_DLL QtNetwork4.dll)
  find_path(GUI_QTSQL_DLL QtSql4.dll)
  find_path(GUI_QTXML_DLL QtXml4.dll)
  find_path(GUI_QTXMLPAT_DLL QtXmlPatterns4.dll)
  find_path(GUI_QTWEBKIT_DLL QtWebKit4.dll)
  find_path(GUI_QTPHONON_DLL phonon4.dll)
  find_path(GUI_QTSQLITE_DLL qsqlite4.dll HINTS ${GUI_QTHELP_DLL}/../plugins/sqldrivers)
  install(FILES ${GUI_ASSISTANT_EXECUTABLE}/assistant.exe DESTINATION ${GUI_BIN_DEST} COMPONENT help)
  install(FILES ${GUI_QTHELP_DLL}/QtHelp4.dll DESTINATION ${GUI_BIN_DEST} COMPONENT help)
  install(FILES ${GUI_QTCLUCENE_DLL}/QtCLucene4.dll DESTINATION ${GUI_BIN_DEST} COMPONENT help)
  install(FILES ${GUI_QTNETWORK_DLL}/QtNetwork4.dll DESTINATION ${GUI_BIN_DEST} COMPONENT help)
  install(FILES ${GUI_QTSQL_DLL}/QtSql4.dll DESTINATION ${GUI_BIN_DEST} COMPONENT help)
  install(FILES ${GUI_QTXML_DLL}/QtXml4.dll DESTINATION ${GUI_BIN_DEST} COMPONENT help)
  install(FILES ${GUI_QTXMLPAT_DLL}/QtXmlPatterns4.dll DESTINATION ${GUI_BIN_DEST} COMPONENT help)
  install(FILES ${GUI_QTWEBKIT_DLL}/QtWebKit4.dll DESTINATION ${GUI_BIN_DEST} COMPONENT help)
  if(GUI_QTPHONON_DLL)
      install(FILES ${GUI_QTPHONON_DLL}/phonon4.dll DESTINATION ${GUI_BIN_DEST} COMPONENT help)
  endif()
  if(GUI_QTSQLITE_DLL)
      install(FILES ${GUI_QTSQLITE_DLL}/qsqlite4.dll DESTINATION ${GUI_BIN_DEST}/plugins/sqldrivers COMPONENT help)
  endif()
endif(WIN32)

install(FILES ${divine_BINARY_DIR}/manual/divine.qhc
              ${divine_BINARY_DIR}/manual/manual.qch
        DESTINATION ${GUI_HELP_DEST} COMPONENT help)
