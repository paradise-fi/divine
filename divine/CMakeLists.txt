include( ../wibble/test.cmake )

file( GLOB SRC_TOP *.cpp )
file( GLOB H_TOP *.h )

file( GLOB SRC_LEGACY legacy/*/*.cc legacy/*/*/*.cc legacy/*/*/*/*.cc
                      legacy/*/*.c legacy/*/*/*.c legacy/*/*/*/*.c )
file( GLOB H_LEGACY legacy/*/*.hh legacy/*/*/*.hh )

file( GLOB SRC_BA ltl2ba/*.cc )
file( GLOB H_BA ltl2ba/*.hh )

file( GLOB H_GEN generator/*.h )
file( GLOB H_ALG algorithm/*.h )

file( GLOB testh *.test.h )

set( SRC_DLFCN "" )
if( WIN32 )
    set( SRC_DLFCN dlfcn-win32.c )
endif( WIN32 )

set( SHA1 ${divine_BINARY_DIR}/divine/sha1 )
set( UPDATE_SHA1 ${divine_SOURCE_DIR}/divine/update-sha1.sh )
set( STRINGIFY ${divine_SOURCE_DIR}/divine/stringify.sh )

add_custom_command(
  COMMAND sh ${UPDATE_SHA1} "${SHA1SUM}" "${divine_SOURCE_DIR}" "${SHA1}"
  OUTPUT "${SHA1}.cpp.fake"
)
set_source_files_properties( "${SHA1}.cpp.fake" PROPERTIES SYMBOLIC ON )
set_source_files_properties( "${SHA1}.cpp" PROPERTIES GENERATED ON )

macro( stringify file )
  string( REPLACE "." "_" fileu "${file}" )
  string( REPLACE "/" "_" fileu "${fileu}" )
  string( REPLACE "-" "_" fileu "${fileu}" )
  add_custom_command(
    OUTPUT ${fileu}_str.cpp
    DEPENDS ${file} ${STRINGIFY}
    COMMAND sh ${STRINGIFY} "${fileu}" "${CMAKE_CURRENT_SOURCE_DIR}/${file}"
    VERBATIM
    )
endmacro( stringify )

stringify( generator/custom-api.h )
stringify( pool.h )
stringify( circular.h )
stringify( blob.h )

include_directories( ${DIVINE_INCLUDES} )
add_definitions( ${MPI_COMPILE_FLAGS} )

add_library( libdivine STATIC
    ${SRC_DLFCN} ${SRC_TOP} ${SRC_LEGACY} ${SRC_BA} "${SHA1}.cpp"
    "${SHA1}.cpp.fake" pool_h_str.cpp circular_h_str.cpp blob_h_str.cpp generator_custom_api_h_str.cpp )
set_target_properties( libdivine PROPERTIES OUTPUT_NAME "divine" )
wibble_add_test( divine-test ${testh} )

if( NOT WIN32 )
  set_target_properties( libdivine PROPERTIES COMPILE_FLAGS -fPIC )
endif( NOT WIN32 )

if( CURSES_FOUND )
  target_link_libraries( libdivine ${CURSES_LIBRARIES} )
endif()
target_link_libraries( libdivine ${MPI_LIBRARIES} )

if( NOT WIN32 )
  target_link_libraries( libdivine dl )
endif( NOT WIN32 )

target_link_libraries( divine-test libdivine wibble )
wibble_check_target( divine-test )

install( TARGETS libdivine DESTINATION lib COMPONENT divine_dev )

install( FILES ${H_TOP} DESTINATION include/divine COMPONENT divine_dev )
install( FILES ${H_LEGACY} DESTINATION include/divine/legacy COMPONENT divine_dev )
install( FILES ${H_BA} DESTINATION include/divine/ltl2ba COMPONENT divine_dev )
install( FILES ${H_GEN} DESTINATION include/divine/generator COMPONENT divine_dev )
install( FILES ${H_ALG} DESTINATION include/divine/algorithm COMPONENT divine_dev )
