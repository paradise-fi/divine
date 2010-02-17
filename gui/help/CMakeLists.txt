find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})

file(GLOB HELP_SOURCE about.txt gui/* divine/* divine/pics/*)

# QT_QCOLLECTIONGENERATOR_EXECUTABLE is defined in CMake 2.8
if(NOT QT_QCOLLECTIONGENERATOR_EXECUTABLE)
  if(WIN32)
    find_program(QT_QCOLLECTIONGENERATOR_EXECUTABLE qcollectiongenerator)
  else(WIN32)
    find_program(QT_QCOLLECTIONGENERATOR_EXECUTABLE qcollectiongenerator.exe)
  endif(WIN32)
endif()

if( QT_QCOLLECTIONGENERATOR_EXECUTABLE )
add_custom_command(OUTPUT help_collection
                   COMMAND ${QT_QCOLLECTIONGENERATOR_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/divine-ide.qhcp
                   DEPENDS ${HELP_SOURCE}
)

add_custom_target(base_help ALL
  DEPENDS help_collection
)

install(FILES divine-ide.qhc divine.qch DESTINATION ${GUI_HELP_DEST} COMPONENT help)

endif()

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
  install(FILES ${GUI_QTPHONON_DLL}/phonon4.dll DESTINATION ${GUI_BIN_DEST} COMPONENT help)
  install(FILES ${GUI_QTSQLITE_DLL}/qsqlite4.dll DESTINATION ${GUI_BIN_DEST}/plugins/sqldrivers COMPONENT help)

  # FIXME: additional hardcoded dependencies
  find_file(GCC_DW_LIB libgcc_s_dw2-1.dll)
  install(FILES ${GCC_DW_LIB} DESTINATION ${GUI_BIN_DEST} COMPONENT help)
endif(WIN32)
