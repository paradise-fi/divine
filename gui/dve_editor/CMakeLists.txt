find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})

include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})

set(HDRS ../include)

set(DVE_EDITOR_SRC
    dve_editor_plugin.cpp
    dve_editor_plugin.h
    dve_highlighter.cpp
    dve_highlighter.h
    prefs_dve.cpp
    prefs_dve.h
)

set(DVE_EDITOR_MOC
    ${HDRS}/plugins.h

    dve_editor_plugin.h
    dve_highlighter.h
    prefs_dve.h
)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/plugins)

qt4_wrap_cpp(DVE_EDITOR_SRC2 ${DVE_EDITOR_MOC})

add_library(dve_editor MODULE ${DVE_EDITOR_SRC} ${DVE_EDITOR_SRC2})
target_link_libraries(dve_editor base_shared ${QT_LIBRARIES})

install(TARGETS dve_editor DESTINATION ${GUI_PLUGIN_DEST} COMPONENT gui_tools)
