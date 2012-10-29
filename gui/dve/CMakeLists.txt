find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})

include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})
include_directories(${DIVINE_INCLUDES})

# HACK: combine hacking
set( MAKE_STRING_CONST "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/make-string-const.pl" )
set( COMBINE_M4 "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/combine.m4" )
set( COMBINE_M4_H "${CMAKE_CURRENT_BINARY_DIR}/combine.m4.h" )

add_custom_command(
  OUTPUT ${COMBINE_M4_H}
  DEPENDS ${COMBINE_M4} ${MAKE_STRING_CONST}
  COMMAND ${PERL_EXECUTABLE} ${MAKE_STRING_CONST} "const char *combine_m4 = " ${COMBINE_M4} > ${COMBINE_M4_H} )

set(DVE_PLUGIN_SRC
    combine.cpp
    combine.h
    combineDialog.cpp
    combineDialog.h
    divineTools.cpp
    divineTools.h
    dvePlugin.h
    dvePlugin.cpp
    dvePlugin.h
    dveDocument.cpp
    dveDocument.h
    dveHighlighter.cpp
    dveHighlighter.h
    dvePreferences.cpp
    dvePreferences.h
    dveSimulator.cpp
    dveSimulator.h
    ltlHighlighter.cpp
    ltlHighlighter.h
    ltlPreferences.cpp
    ltlPreferences.h
)

set(DVE_PLUGIN_MOC
    ../baseIde/plugins.h

    divineTools.h
    combineDialog.h
    dvePlugin.h
    dveDocument.h
    dveHighlighter.h
    dvePreferences.h
    dveSimulator.h
    ltlDocument.h
    ltlHighlighter.h
    ltlPreferences.h
)

set(DVE_PLUGIN_UI
    ui/combineDialog.ui
)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/plugins)

qt4_wrap_cpp(DVE_PLUGIN_SRC2 ${DVE_PLUGIN_MOC})
qt4_wrap_ui(DVE_PLUGIN_SRC3 ${DVE_PLUGIN_UI})

add_library(dvePlugin MODULE ${DVE_PLUGIN_SRC} ${DVE_PLUGIN_SRC2} ${DVE_PLUGIN_SRC3} ${COMBINE_M4_H})
target_link_libraries(dvePlugin baseIDE libdivine wibble ${QT_LIBRARIES})

install(TARGETS dvePlugin DESTINATION ${GUI_PLUGIN_DEST} COMPONENT gui_tools)
