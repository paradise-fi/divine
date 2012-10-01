set( LEX ${CMAKE_CURRENT_BINARY_DIR}/mulexer.cpp )
set( PARSE ${CMAKE_CURRENT_BINARY_DIR}/muparser.cpp )

file( GLOB code *.C )
byacc_target( muparser mu.y ${PARSE} )
flex_target( mulexer mu.l ${LEX} )

include_directories( ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} )

set( STRINGIFY ${CMAKE_CURRENT_SOURCE_DIR}/stringify.sh )
set( SUPPORT_H ${CMAKE_CURRENT_SOURCE_DIR}/support_h.sh )

macro( stringify file )
  string( REPLACE "." "_" fileu "${file}" )
  add_custom_command(
    OUTPUT ${fileu}_str.cpp
    DEPENDS ${file} ${STRINGIFY}
    COMMAND sh ${STRINGIFY} "${fileu}" "${CMAKE_CURRENT_SOURCE_DIR}/${file}"
    VERBATIM
    )
endmacro( stringify )

macro( support file )
  stringify( "support/${file}" )
  string( REPLACE "." "_" fileu "${file}" )
  set( SUPPORT "support/${fileu}_str.cpp" "${SUPPORT}" )
endmacro( support )

support( mu_statecl.h )
support( mu_state.h )
support( mu_verifier.h )
support( mu_system.h )
support( mu_util.h )
support( mu_util_dep.h )
support( mu_sym.h )
support( mu_io.h )

support( mu_state.C )
support( mu_verifier.C )
support( mu_system.C )
support( mu_util.C )
support( mu_sym.C )
support( mu_io.C )

add_custom_command(
  OUTPUT support.h
  DEPENDS ${SUPPORT_H}
  COMMAND sh ${SUPPORT_H} "${CMAKE_CURRENT_BINARY_DIR}/support.h" "${SUPPORT}"
  VERBATIM
)

add_library( murphi STATIC ${code} ${PARSE} ${LEX} ${SUPPORT} support.h )

