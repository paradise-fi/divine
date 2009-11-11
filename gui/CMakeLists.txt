project(gui)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

set(GUI_LIB_PATH ${CMAKE_INSTALL_PREFIX}/lib/divine)
set(GUI_PLUGIN_PATH ${CMAKE_INSTALL_PREFIX}/lib/divine/gui_plugins)
set(GUI_BIN_PATH ${CMAKE_INSTALL_PREFIX}/bin)

include_directories(include)
add_subdirectory(main)
add_subdirectory(base_shared)
add_subdirectory(base_tools)
add_subdirectory(dve_editor)
add_subdirectory(dve_divine2)
add_subdirectory(ltl_editor)
