set( T sh ${CMAKE_CURRENT_SOURCE_DIR}/libdivine-check )

file( GLOB TEST_SOURCES test_*.cpp )

include_directories( ${divine_SOURCE_DIR} ${divine_BINARY_DIR} ${BOOST_INCLUDE_PATH} ${MPI_INCLUDE_PATH} ${divine_SOURCE_DIR}/divine/legacy )

wibble_check_target( libdivine-test )
# add_custom_target( check COMMAND make libdivine-test && ${T}
#                    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/data-stamp )

add_executable( libdivine-test tut-main.cpp ${TEST_SOURCES} )
target_link_libraries( libdivine-test divine wibble pthread )

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/data-stamp
  COMMAND touch data-stamp )

#ADD_CUSTOM_COMMAND(
  #OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tut-syms.cpp
  #DEPENDS ${wibble_BINARY_DIR}/libwibble.a
  #COMMAND sh ${CMAKE_SOURCE_DIR}/extract_syms.sh ${wibble_BINARY_DIR}/libwibble.a wibble_sym_references > tut-syms.cpp )
