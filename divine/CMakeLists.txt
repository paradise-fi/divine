with_warnings()

include( ../cmake/stringify.cmake )
include( ../cmake/template-separe.cmake )

file( GLOB SRC *.cpp )
file( GLOB H *.h )

foreach( D utility toolkit generator algorithm instances explicit )
    file( GLOB SRC_${D} ${D}/*.cpp )
    file( GLOB H_${D} ${D}/*.h )
    install( FILES ${H_${D}} DESTINATION include/divine/${D} COMPONENT divine_dev )
    if ( NOT ${D} STREQUAL instances )
       list( APPEND SRC ${SRC_${D}} )
       list( APPEND H ${H_${D}} )
    endif()
endforeach()

# generated instantiate files
set( H_INSTANCES_EXTERN "${CMAKE_CURRENT_BINARY_DIR}/instances/auto/extern.h" )
install( FILES "${H_INSTANCES_EXTERN}" DESTINATION include/divine/instances/auto COMPONENT divine_dev )

if( ${BUILD_SHARED_LIBS} )
    set( LIBDIVINE_INSTANCE_BUCKETS 10 )
else()
    set( LIBDIVINE_INSTANCE_BUCKETS 1 )
endif()

foreach( I RANGE 1 ${MAX_INSTANCE_FILES_COUNT} )
    math( EXPR bucket "${I} % ${LIBDIVINE_INSTANCE_BUCKETS} + 1" )
    set( F "${CMAKE_CURRENT_BINARY_DIR}/instances/auto/instances-${I}.cpp" )
    list( APPEND SRC_auto_instances ${F} )
    list( APPEND SRC_auto_instances_${bucket} ${F} )
    set_source_files_properties( ${F} PROPERTIES GENERATED ON )
endforeach()

if( GEN_COIN )
    file( GLOB SRC_COIN coin/*.cpp coin/parse/*.cpp)
    set_source_files_properties( coin/parse/lex.coin.cpp PROPERTIES COMPILE_DEFINITIONS YY_NO_INPUT=1 )
endif()

if( GEN_LLVM )
    file( GLOB SRC_LLVM llvm/*.cpp )
    file( GLOB H_LLVM llvm/*.h )
    add_subdirectory( llvm/userspace )
    stringify( "llvm" llvm/problem.def )
    template_separe( LLVM llvm/interpreter.cpp )
else() # enable silk even when LLVM is disabled
    file( GLOB SRC_LLVM llvm/silk-*.cpp )
    file( GLOB H_LLVM llvm/silk-*.h )
endif()

if( GEN_DVE )
    file( GLOB SRC_DVE dve/*.cpp )
endif()

if ( GEN_TIMED )
    file( GLOB SRC_TIMED timed/*.cpp )
    include_directories( ${Boost_INCLUDE_DIR} )
endif()

file( GLOB SRC_BA ltl2ba/*.cpp )
file( GLOB H_BA ltl2ba/*.hh )

set( SRC_DLFCN "" )
if( WIN32 )
    set( SRC_DLFCN ../external/dlfcn-win32/dlfcn.c )
endif()

set( VERSION_FILE ${divine_BINARY_DIR}/divine/version-generated.cpp )
set( UPDATE_VERSION ${divine_SOURCE_DIR}/divine/update-version.sh )

add_custom_command(
  COMMAND sh ${UPDATE_VERSION} "${SHA1SUM}" "${divine_SOURCE_DIR}" "${VERSION_FILE}" "${DIVINE_VERSION}"
  OUTPUT "${VERSION_FILE}.check"
)
set_source_files_properties( "${VERSION_FILE}.check" PROPERTIES SYMBOLIC ON )
set_source_files_properties( "${VERSION_FILE}" PROPERTIES GENERATED ON )

add_custom_command(
  OUTPUT flags-generated.cpp
  VERBATIM
  COMMAND sh -c "echo \"const char* divineCompileFlags = \\\"${BUILDOPTS}\\\";\">flags-generated.cpp"
)

stringify( "" toolkit/pool.h )
stringify( "" toolkit/blob.h )
stringify( "" cesmi/usr-cesmi.h )
stringify( "" cesmi/usr-cesmi.cpp )
stringify( "" cesmi/usr-ltl.h )
stringify( "" cesmi/usr-ltl.cpp )

set( GENERATED_INSTANCES_DIR "${CMAKE_CURRENT_BINARY_DIR}/instances/auto" )
add_executable( instances-generator instances/generator.cpp )
file( MAKE_DIRECTORY ${GENERATED_INSTANCES_DIR} )
add_custom_command( TARGET instances-generator POST_BUILD
    COMMAND "${CMAKE_CURRENT_BINARY_DIR}/instances-generator" ${MAX_INSTANCE_FILES_COUNT} ${MIN_INSTANCES_PER_FILE}
    WORKING_DIRECTORY ${GENERATED_INSTANCES_DIR}
    COMMENT "Generating instance files"
    VERBATIM )
target_link_libraries( instances-generator libdivine )

include_directories( ${DIVINE_INCLUDES} )
include_directories( SYSTEM ${DIVINE_SYS_INCLUDES} )
add_definitions( ${MPI_COMPILE_FLAGS} ${LLVM_COMPILE_FLAGS} )

add_library( libdivine
    ${SRC_DLFCN} ${SRC} ${SRC_TIMED}
    ${SRC_COIN} ${SRC_BA} ${SRC_LLVM} ${SRC_DVE}
    instances/common.cpp
    ${LLVM_TS_OUTPUTS}
    "${VERSION_FILE}" "${VERSION_FILE}.check" "flags-generated.cpp"
    ${STRINGIFIED} )

target_link_libraries( libdivine pthread )
set_target_properties( libdivine PROPERTIES OUTPUT_NAME "divine" )

set_source_files_properties( "${CMAKE_CURRENT_BINARY_DIR}/instances/auto/select.cpp"
                             PROPERTIES GENERATED ON )
add_library( libdivine-instances instances/select.cpp
                                 "${CMAKE_CURRENT_BINARY_DIR}/instances/auto/select.cpp" )
add_dependencies( libdivine-instances instances-generator )
set_target_properties( libdivine-instances PROPERTIES OUTPUT_NAME "divine-instances" )
list( APPEND LIBDIVINE_INSTANCES libdivine-instances )

foreach( I RANGE 1 ${LIBDIVINE_INSTANCE_BUCKETS} )
    add_library( libdivine-instances-${I} instances/select.cpp ${SRC_auto_instances_${I}} )
    add_dependencies( libdivine-instances-${I} instances-generator )
    set_target_properties( libdivine-instances-${I} PROPERTIES OUTPUT_NAME "divine-instances-${I}" )
    list( APPEND LIBDIVINE_INSTANCES libdivine-instances-${I} )
endforeach()

set( LIBDIVINE_INSTANCES ${LIBDIVINE_INSTANCES} PARENT_SCOPE )

bricks_unittest( test-divine ${H} ${H_LLVM} )

if( NOT WIN32 )
  set_target_properties( libdivine PROPERTIES COMPILE_FLAGS -fPIC )
endif()

if( GEN_LLVM )
  target_link_libraries( libdivine ${LLVM_LIBRARIES} )
  target_link_libraries( libdivine ${LLVM_SYSLIBS} )
  target_link_libraries( libdivine pdclib_str )
  target_link_libraries( libdivine m_str )
  target_link_libraries( libdivine cxxabi_str )
  target_link_libraries( libdivine cxx_str )
  target_link_libraries( libdivine llvm_usr_str )
endif()

if( OPT_CURSES )
  target_link_libraries( libdivine ${CURSES_LIBRARIES} )
endif()

target_link_libraries( libdivine ${MPI_LIBRARIES} )

if( OPT_LTL3BA )
  target_link_libraries( libdivine ltl3ba )
endif()

if( GEN_TIMED )
  target_link_libraries( libdivine dbm )
  target_link_libraries( libdivine utap )
endif()

if ( OPT_SQL )
    target_link_libraries( libdivine nanodbc )
endif()

target_link_libraries( test-divine libdivine )

if( NOT WIN32 )
  target_link_libraries( libdivine dl )
else()
    target_link_libraries( libdivine psapi )
endif()

add_custom_target( unit_divine
    COMMAND sh ${TEST_WRAPPER} ${WITH_LCOV} ${CMAKE_CURRENT_BINARY_DIR}/test-divine
    VERBATIM
    DEPENDS test-divine )
add_dependencies( unit unit_divine )

install( TARGETS libdivine DESTINATION lib COMPONENT divine_dev )

install( FILES ${H} DESTINATION include/divine COMPONENT divine_dev )
install( FILES ${H_BA} DESTINATION include/divine/ltl2ba COMPONENT divine_dev )
