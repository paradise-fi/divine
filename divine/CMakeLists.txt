appendFlag( "CXX;C" "-Wall" )
appendFlag( "CXX;C" "-Wextra" )
appendFlag( "CXX;C" "-Wno-unused-parameter" )
appendFlag( "CXX;C" "-Wno-sign-compare" )
appendFlag( "CXX;C" "-Wno-missing-field-initializers" )
appendFlag( "CXX" "-Wno-overloaded-virtual" )

include( ../wibble/test.cmake )

file( GLOB SRC *.cpp )
file( GLOB H *.h )

foreach( D utility toolkit generator algorithm instances )
    file( GLOB SRC_${D} ${D}/*.cpp )
    file( GLOB H_${D} ${D}/*.h )
    install( FILES ${H_${D}} DESTINATION include/divine/${D} COMPONENT divine_dev )
    if ( NOT ${D} STREQUAL instances )
       list( APPEND SRC ${SRC_${D}} )
    endif()
endforeach()

if ( LEGACY )
   file( GLOB SRC_LEGACY legacy/*/*.cc legacy/*/*/*.cc legacy/*/*/*/*.cc
                         legacy/*/*.c legacy/*/*/*.c legacy/*/*/*/*.c )
   file( GLOB H_LEGACY legacy/*/*.hh legacy/*/*/*.hh )
endif()

if( COIN )
    file( GLOB SRC_COIN coin/*.cpp coin/parse/*.cpp)
endif( COIN )

if( LLVM )
    file( GLOB SRC_LLVM llvm/*.cpp )
    file( GLOB llvmtesth llvm/*.test.h )
endif( LLVM )

if( DVE )
    file( GLOB SRC_DVE dve/*.cpp )
endif( DVE )

if ( TIMED )
    file( GLOB SRC_TIMED timed/*.cpp )
endif( TIMED )

file( GLOB SRC_BA ltl2ba/*.cpp )
file( GLOB H_BA ltl2ba/*.hh )

file( GLOB testh *.test.h toolkit/*.test.h graph/*.test.h )

set( SRC_DLFCN "" )
if( WIN32 )
    set( SRC_DLFCN ../external/dlfcn-win32/dlfcn.c )
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

get_property( DIVINE_COMPILE_FLAGS DIRECTORY PROPERTY COMPILE_DEFINITIONS )
string(REPLACE ";" " " DCF_OUT "${DIVINE_COMPILE_FLAGS}")
add_custom_command(
  OUTPUT divinecompileflags.cpp
  VERBATIM
  COMMAND sh -c "echo -n \"const char* divineCompileFlags = \\\"${DCF_OUT}\\\";\">divinecompileflags.cpp"
)

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

stringify( generator/cesmi-client.h )
stringify( toolkit/pool.h )
stringify( toolkit/blob.h )

include_directories( ${DIVINE_INCLUDES} )
add_definitions( ${MPI_COMPILE_FLAGS} ${LLVM_COMPILE_FLAGS} )
set_source_files_properties( ${SRC_LLVM} PROPERTIES COMPILE_FLAGS "-fno-rtti" )

add_library( libdivine STATIC
    ${SRC_DLFCN} ${SRC} ${SRC_LEGACY} ${SRC_TIMED}
    ${SRC_COIN} ${SRC_BA} ${SRC_LLVM} ${SRC_DVE}
    "${SHA1}.cpp" "${SHA1}.cpp.fake" "divinecompileflags.cpp"
    toolkit_pool_h_str.cpp toolkit_blob_h_str.cpp generator_cesmi_client_h_str.cpp )

add_library( libdivine-instances STATIC ${SRC_instances} )

set_target_properties( libdivine PROPERTIES OUTPUT_NAME "divine" )
set_target_properties( libdivine-instances PROPERTIES OUTPUT_NAME "divine-instances" )

wibble_add_test( divine-test ${testh} ${llvmtesth} )

if( NOT WIN32 )
  set_target_properties( libdivine PROPERTIES COMPILE_FLAGS -fPIC )
endif( NOT WIN32 )

if( LLVM )
  target_link_libraries( libdivine ${LLVM_LIBRARIES} )
endif()

if( CURSES )
  target_link_libraries( libdivine ${CURSES_LIBRARIES} )
endif()

target_link_libraries( libdivine ${MPI_LIBRARIES} )

if( LTL3BA )
  target_link_libraries( libdivine ltl3ba )
endif( LTL3BA )

if( TIMED )
  target_link_libraries( libdivine dbm )
  target_link_libraries( libdivine utap )
endif()

target_link_libraries( divine-test libdivine wibble )

if( NOT WIN32 )
  target_link_libraries( libdivine dl )
  wibble_check_target( divine-test )
endif( NOT WIN32 )

install( TARGETS libdivine DESTINATION lib COMPONENT divine_dev )
install( TARGETS libdivine-instances DESTINATION lib COMPONENT divine_dev )

install( FILES ${H} DESTINATION include/divine COMPONENT divine_dev )
install( FILES ${H_LEGACY} DESTINATION include/divine/legacy COMPONENT divine_dev )
install( FILES ${H_BA} DESTINATION include/divine/ltl2ba COMPONENT divine_dev )

