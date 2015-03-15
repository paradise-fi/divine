find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})

include_directories(${QT_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})
add_definitions( ${DIVINE_INCLUDE_DEFS} )

# HACK: combine hacking
set( MAKE_STRING_CONST "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/make-string-const.pl" )
set( COMBINE_M4 "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/combine.m4" )
set( COMBINE_M4_H "${CMAKE_CURRENT_BINARY_DIR}/combine.m4.h" )

add_custom_command(
  OUTPUT ${COMBINE_M4_H}
  DEPENDS ${COMBINE_M4} ${MAKE_STRING_CONST}
  COMMAND ${PERL_EXECUTABLE} ${MAKE_STRING_CONST} "const char *combine_m4 = " ${COMBINE_M4} > ${COMBINE_M4_H} )

set(DVE_MODULE_SRC
    combine.cpp
    combine.h
    combineDialog.cpp
    combineDialog.h
    dveModule.cpp
    dveModule.h
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

set(DVE_MODULE_MOC
    combineDialog.h
    dveModule.h
    dveDocument.h
    dveHighlighter.h
    dvePreferences.h
    dveSimulator.h
    ltlDocument.h
    ltlHighlighter.h
    ltlPreferences.h
)

set(DVE_MODULE_UI
    ui/combineDialog.ui
)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/plugins)

qt4_wrap_cpp(DVE_MODULE_SRC2 ${DVE_MODULE_MOC})
qt4_wrap_ui(DVE_MODULE_SRC3 ${DVE_MODULE_UI})

add_library(dveModule STATIC ${DVE_MODULE_SRC} ${DVE_MODULE_SRC2} ${DVE_MODULE_SRC3} ${COMBINE_M4_H})
target_link_libraries(dveModule baseIDE qtcolorpicker libdivine)
