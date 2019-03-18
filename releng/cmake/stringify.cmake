set( MODPATH ${CMAKE_SOURCE_DIR}/releng/cmake/ )

macro( stringify namespace root path )
  get_filename_component( absdir "${root}" ABSOLUTE BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR} )
  get_filename_component( abspath "${path}" ABSOLUTE BASE_DIR ${absdir} )
  file( RELATIVE_PATH file "${absdir}" "${abspath}" )
  string( REGEX REPLACE "[./+-]" "_" outfile "${file}" )
  list( APPEND ${namespace}_NAMES "${file}" )
  list( APPEND ${namespace}_FILES "str_${outfile}.cpp" )

  add_custom_command(
    OUTPUT str_${outfile}.cpp
    DEPENDS ${abspath} ${MODPATH}/stringify.pl ${ARGN}
    COMMAND perl ${MODPATH}/stringify.pl "${namespace}" "${outfile}" "${abspath}"
    VERBATIM
  )
endmacro( stringify )

macro( stringlist namespace out )
  add_custom_command(
    OUTPUT ${out}_list.cpp
    DEPENDS ${MODPATH}/stringlist.pl
    COMMAND perl ${MODPATH}/stringlist.pl ${namespace} "${out}" ${${namespace}_NAMES}
    VERBATIM
  )
endmacro( stringlist )
