with_warnings()

include( ../cmake/stringify.cmake )
include( ../cmake/template-separe.cmake )

file( GLOB CPP *.cpp )
file( GLOB HPP *.h *.hpp )

foreach( D utility llvm compile toolkit statespace algorithm ui )
    file( GLOB CPP_${D} ${D}/*.cpp )
    file( GLOB HPP_${D} ${D}/*.hpp )
    install( FILES ${HPP_${D}} DESTINATION include/divine/${D} )
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
    set_property( TARGET libdivine-instances-${I} PROPERTY JOB_POOL_COMPILE instances )
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

if ( OPT_SQL )
    target_link_libraries( libdivine nanodbc )
endif()

target_link_libraries( test-divine libdivine )

if( ${CMAKE_SYSTEM_NAME} STREQUAL "Linux" )
  target_link_libraries( libdivine dl )
endif()

if( WIN32 )
  target_link_libraries( libdivine psapi )
endif()

add_custom_target( unit_divine
    COMMAND sh ${TEST_WRAPPER} ${WITH_LCOV} ${CMAKE_CURRENT_BINARY_DIR}/test-divine
    VERBATIM
    USES_TERMINAL
    DEPENDS test-divine )
add_dependencies( unit unit_divine )

install( TARGETS libdivine ${LIBDIVINE_INSTANCES} DESTINATION lib COMPONENT divine_dev )

install( FILES ${H} DESTINATION include/divine COMPONENT divine_dev )
install( FILES ${H_BA} DESTINATION include/divine/ltl2ba COMPONENT divine_dev )
