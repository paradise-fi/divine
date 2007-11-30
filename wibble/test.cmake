include(FindPerl)

macro( wibble_add_test name )
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${name}-generated.cpp"
    DEPENDS ${wibble_SOURCE_DIR}/test-genrunner.pl ${ARGN}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND ${PERL_EXECUTABLE} ${wibble_SOURCE_DIR}/test-genrunner.pl ${ARGN} > ${CMAKE_CURRENT_BINARY_DIR}/${name}-generated.cpp
  )
  set_source_files_properties( ${CMAKE_CURRENT_BINARY_DIR}/${name}-generated.cpp
    PROPERTIES GENERATED ON )
  add_executable( ${name} ${CMAKE_CURRENT_BINARY_DIR}/${name}-generated.cpp )
endmacro( wibble_add_test )

macro( wibble_check_target tgt )
  add_custom_target( check COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${tgt}
    DEPENDS ${ARGV} )
endmacro( wibble_check_target )
