find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})

include_directories( ${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR} )
include_directories( ${DIVINE_INCLUDES} )

set(HDRS ../include)

set(DVE_DIVINE2_SRC
    dve_divine2_plugin.cpp
    dve_divine2_plugin.h
    dve_simulator.cpp
    dve_simulator.h
    debug_system.cpp
    debug_system.h
    combine_dlg.cpp
    combine_dlg.h
    prefs_divine.cpp
    prefs_divine.h
)

set(DVE_DIVINE2_MOC
    ${HDRS}/plugins.h
    dve_divine2_plugin.h
    dve_simulator.h
    combine_dlg.h
    prefs_divine.h
)

set(DVE_DIVINE2_UI
    ui/combine.ui
    ui/prefs_divine.ui
)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/plugins)

qt4_wrap_cpp(DVE_DIVINE2_SRC2 ${DVE_DIVINE2_MOC})
qt4_wrap_ui(DVE_DIVINE2_SRC3 ${DVE_DIVINE2_UI})

add_library(dve_divine2 MODULE ${DVE_DIVINE2_SRC} ${DVE_DIVINE2_SRC2} ${DVE_DIVINE2_SRC3})
target_link_libraries(dve_divine2 base_shared libdivine wibble ${QT_LIBRARIES})

install(TARGETS dve_divine2 DESTINATION ${GUI_PLUGIN_DEST} COMPONENT gui_tools)
