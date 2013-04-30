set( STRINGIFY ${CMAKE_SOURCE_DIR}/cmake/stringify.sh )

macro( stringify file )
  string( REPLACE "." "_" fileu "${file}" )
  string( REPLACE "/" "_" fileu "${fileu}" )
  string( REPLACE "+" "_" fileu "${fileu}" )
  string( REPLACE "-" "_" fileu "${fileu}" )
  add_custom_command(
    OUTPUT ${fileu}_str.cpp
    DEPENDS ${file} ${STRINGIFY}
    COMMAND sh ${STRINGIFY} "${CMAKE_CURRENT_SOURCE_DIR}" "${file}"
    VERBATIM
  )
  list( APPEND STRINGIFIED_FILES "${file}" )
  list( APPEND STRINGIFIED "${fileu}_str.cpp" )
endmacro( stringify )

macro( stringlist out )
  add_custom_command(
    OUTPUT ${out}_list.cpp
    DEPENDS ${STRINGIFY}
    COMMAND sh ${STRINGIFY} "-l" "${out}" ${STRINGIFIED_FILES}
    VERBATIM
  )
endmacro( stringlist )
