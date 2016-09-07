set( STRINGIFY ${CMAKE_SOURCE_DIR}/releng/cmake/stringify.pl )

macro( stringifyInDir namespace dir path )
  get_filename_component( absdir "${dir}" ABSOLUTE BASE_DIR ${CMAKE_CURRENT_BINARY_DIR} )
  get_filename_component( abspath "${path}" ABSOLUTE BASE_DIR ${CMAKE_CURRENT_BINARY_DIR} )
  file( RELATIVE_PATH file "${absdir}" "${abspath}" )

  string( REPLACE "." "_" fileu "${file}" )
  string( REPLACE "/" "_" fileu "${fileu}" )
  string( REPLACE "+" "_" fileu "${fileu}" )
  string( REPLACE "-" "_" fileu "${fileu}" )

  add_custom_command(
    OUTPUT ${fileu}_str.cpp
    DEPENDS ${abspath} ${STRINGIFY}
    COMMAND perl ${STRINGIFY} "${namespace}" "${dir}" "${file}"
    VERBATIM
  )
  list( APPEND STRINGIFIED_FILES "${file}" )
  list( APPEND STRINGIFIED "${fileu}_str.cpp" )
endmacro( stringifyInDir )

macro( stringify namespace file )
    stringifyInDir( ${namespace} ${CMAKE_CURRENT_SOURCE_DIR} ${file} )
endmacro( stringify )

macro( stringlist namespace out )
  add_custom_command(
    OUTPUT ${out}_list.cpp
    DEPENDS ${STRINGIFY}
    COMMAND perl ${STRINGIFY} ${namespace} "-l" "${out}" ${STRINGIFIED_FILES}
    VERBATIM
  )
endmacro( stringlist )
