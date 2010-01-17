find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})

include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})
include_directories(qt)

set(HDRS ../include)

set(BASE_TOOLS_SRC
    base_tools_plugin.cpp
    base_tools.h
    output.cpp
    output.h
    prefs_editor.cpp
    prefs_editor.h
    prefs_output.cpp
    prefs_output.h
    prefs_simulator.cpp
    prefs_simulator.h
    prefs_trace.cpp
    prefs_trace.h
    sequence.cpp
    sequence.h
    trace.cpp
    trace.h
    transitions.cpp
    transitions.h
    watch.cpp
    watch.h
)

set(BASE_TOOLS_MOC
    ${HDRS}/plugins.h
    base_tools.h
    output.h
    prefs_output.h
    prefs_simulator.h
    prefs_trace.h
    sequence.h
    trace.h
    transitions.h
    watch.h
)

set(BASE_TOOLS_UI
    ui/prefs_output.ui
    ui/prefs_editor.ui
    ui/prefs_simulator.ui
    ui/prefs_trace.ui
)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/plugins)

qt4_wrap_cpp(BASE_TOOLS_SRC2 ${BASE_TOOLS_MOC})
qt4_wrap_ui(BASE_TOOLS_SRC3 ${BASE_TOOLS_UI})

add_library(base_tools MODULE ${BASE_TOOLS_SRC} ${BASE_TOOLS_SRC2} ${BASE_TOOLS_SRC3})
target_link_libraries(base_tools base_shared ${QT_LIBRARIES} qtcolorpicker)

install(TARGETS base_tools DESTINATION ${GUI_PLUGIN_DEST} COMPONENT gui_tools)
