project(gui)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
add_definitions( ${MPI_COMPILE_FLAGS} ${LLVM_COMPILE_FLAGS} )

if(WIN32)
  set(GUI_BIN_DEST ./)
  set(GUI_LIB_DEST ./)
  set(GUI_LIBEXEC_DEST ./)
  set(GUI_PLUGIN_DEST plugins)
  set(GUI_HELP_DEST help)
else(WIN32)
  set(GUI_BIN_DEST bin)
  set(GUI_LIB_DEST lib/divine)
  set(GUI_LIBEXEC_DEST libexec/divine)
  set(GUI_PLUGIN_DEST lib/divine/gui_plugins)
  set(GUI_HELP_DEST share/divine/help)
endif(WIN32)

# if(WIN32)
#   add_definitions(-static-libgcc)
# endif(WIN32)

add_subdirectory(qtcolorpicker)

include_directories(include qtcolorpicker)

add_subdirectory(main)
add_subdirectory(base_shared)
add_subdirectory(base_tools)
add_subdirectory(dve_editor)
add_subdirectory(dve_divine2)
add_subdirectory(ltl_editor)

add_subdirectory(help)

# Install the shared Qt component
if(WIN32)
  find_path(GUI_QTCORE_DLL QtCore4.dll)
  find_path(GUI_QTGUI_DLL QtGui4.dll)
  find_file(MINGW_LIB libmingwm10.dll)

  install(FILES ${GUI_QTCORE_DLL}/QtCore4.dll DESTINATION ${GUI_BIN_DEST} COMPONENT qt)
  install(FILES ${GUI_QTGUI_DLL}/QtGui4.dll DESTINATION ${GUI_BIN_DEST} COMPONENT qt)
  install(FILES ${MINGW_LIB} DESTINATION ${GUI_BIN_DEST} COMPONENT qt)
endif(WIN32)
