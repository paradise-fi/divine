include( ../wibble/test.cmake )

file( GLOB SRC_TOP *.cpp )
file( GLOB H_TOP *.h )

file( GLOB SRC_LEGACY legacy/*/*.cc legacy/*/*/*.cc legacy/*/*/*/*.cc
                      legacy/*/*.c legacy/*/*/*.c legacy/*/*/*/*.c )
file( GLOB H_LEGACY legacy/*/*.hh legacy/*/*/*.hh )

file( GLOB SRC_BA ltl2ba/*.cc )
file( GLOB H_BA ltl2ba/*.hh )

file( GLOB testh *.test.h )

set( CONTEXT ${divine_BINARY_DIR}/divine/context.sha1 )
set( UPDATE_CONTEXT ${divine_SOURCE_DIR}/divine/update-context.sh )

add_custom_command(
  COMMAND sh ${UPDATE_CONTEXT} ${SHA1SUM} ${divine_SOURCE_DIR} ${CONTEXT}
  OUTPUT ${CONTEXT}.bwah
)

set_source_files_properties( ${CONTEXT}.bwah PROPERTIES SYMBOLIC ON )

include_directories( ${DIVINE_INCLUDES} )
add_definitions( ${MPI_COMPILE_FLAGS} )

add_library( libdivine STATIC
    ${SRC_TOP} ${SRC_LEGACY} ${SRC_BA} ${CONTEXT}.bwah )
set_target_properties( libdivine PROPERTIES OUTPUT_NAME "divine" )
wibble_add_test( divine-test ${testh} )
target_link_libraries( libdivine dl ${MPI_LIBRARIES} )
target_link_libraries( divine-test divine wibble )
wibble_check_target( divine-test )
