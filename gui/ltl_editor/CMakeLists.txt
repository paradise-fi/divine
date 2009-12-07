find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})

include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})

set(HDRS ../include)

set(LTL_EDITOR_SRC
    ltl_editor_plugin.cpp
    ltl_editor_plugin.h
    ltl_highlighter.cpp
    ltl_highlighter.h
    prefs_ltl.cpp
    prefs_ltl.h
)

set(LTL_EDITOR_MOC
    ${HDRS}/plugins.h

    ltl_editor_plugin.h
    ltl_highlighter.h
    prefs_ltl.h
)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/plugins)

qt4_wrap_cpp(LTL_EDITOR_SRC2 ${LTL_EDITOR_MOC})

add_library(ltl_editor MODULE ${LTL_EDITOR_SRC} ${LTL_EDITOR_SRC2})
target_link_libraries(ltl_editor base_shared ${QT_LIBRARIES})

install(TARGETS ltl_editor DESTINATION ${GUI_PLUGIN_DEST} COMPONENT gui_tools)
