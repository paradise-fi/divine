include(FindPerl)

macro( wibble_add_test name )
  string( REPLACE ".test.h" ".cpp" SOURCES "${ARGN}" )
  set( SOURCES ";${SOURCES}" )
  string( REPLACE "/" "_" SOURCES "${SOURCES}" )
  set( prefix "${CMAKE_CURRENT_BINARY_DIR}/${name}-generated-" )
  string( REPLACE ";" ";${prefix}" SOURCES "${SOURCES}" )
  string( REGEX REPLACE "^;" "" SOURCES "${SOURCES}" )
    
  set( main "${prefix}main.cpp" )
  message( STATUS "${SOURCES}" )
  set( generator "${wibble_SOURCE_DIR}/test-genrunner.pl" )

  set( HDRS "${ARGN}" )

  list( LENGTH SOURCES SOURCE_N )
  math( EXPR SOURCE_N "${SOURCE_N} - 1" )
  foreach( i RANGE ${SOURCE_N} )
    LIST( GET HDRS ${i} HDR )
    LIST( GET SOURCES ${i} SRC )
    message( STATUS "${i}: adding ${HDR}  -> ${SRC}" )
    add_custom_command(
      OUTPUT ${SRC}
      DEPENDS ${wibble_SOURCE_DIR}/test-genrunner.pl ${HDR}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND ${PERL_EXECUTABLE} ${generator} ${prefix} header ${HDR} > ${SRC}
    )
  endforeach( i )

  add_custom_command(
    OUTPUT ${main}
    DEPENDS ${wibble_SOURCE_DIR}/test-genrunner.pl ${ARGN}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${PERL_EXECUTABLE} ${generator} ${prefix} main ${ARGN} > ${main}
  )

  set_source_files_properties( ${SOURCES} ${main} PROPERTIES GENERATED ON )
  add_executable( ${name} ${SOURCES} ${main} )
endmacro( wibble_add_test )

macro( wibble_check_target tgt )
  add_custom_target( check COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${tgt}
    DEPENDS ${ARGV} )
endmacro( wibble_check_target )
