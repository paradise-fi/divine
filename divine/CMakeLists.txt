with_warnings()

include( ../wibble/test.cmake )
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
    endif()
endforeach()

# generated instantiate files
set( H_INSTANCES_EXTERN "${CMAKE_CURRENT_BINARY_DIR}/instances/auto/extern.h" )
install( FILES "${H_INSTANCES_EXTERN}" DESTINATION include/divine/instances/auto COMPONENT divine_dev )
list( APPEND SRC_auto_instances "${CMAKE_CURRENT_BINARY_DIR}/instances/auto/select.cpp" )
foreach( I RANGE 1 ${MAX_INSTANCE_FILES_COUNT} )
    set( F "${CMAKE_CURRENT_BINARY_DIR}/instances/auto/instances-${I}.cpp" )
    list( APPEND SRC_auto_instances ${F} )
endforeach()
set_source_files_properties( ${SRC_auto_instances} PROPERTIES GENERATED ON )

if( GEN_COIN )
    file( GLOB SRC_COIN coin/*.cpp coin/parse/*.cpp)
    set_source_files_properties( coin/parse/lex.coin.cpp PROPERTIES COMPILE_DEFINITIONS YY_NO_INPUT=1 )
endif()

if( GEN_LLVM )
    file( GLOB SRC_LLVM llvm/*.cpp )
    file( GLOB llvmtesth llvm/*.test.h )
    add_subdirectory( llvm/userspace )
    stringify( "llvm" llvm/problem.def )
    template_separe( LLVM llvm/interpreter.cpp )
else() # enable silk even when LLVM is disabled
    file( GLOB SRC_LLVM llvm/silk-*.cpp )
    file( GLOB llvmtesth llvm/silk-*.test.h )
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

file( GLOB testh *.test.h toolkit/*.test.h graph/*.test.h explicit/*.test.h )

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
  COMMAND sh -c "echo -n \"const char* divineCompileFlags = \\\"${BUILDOPTS}\\\";\">flags-generated.cpp"
)

stringify( "" toolkit/pool.h )
stringify( "" toolkit/blob.h )
stringify( "" cesmi/usr-cesmi.h )
stringify( "" cesmi/usr-cesmi.cpp )
stringify( "" cesmi/usr-ltl.h )
stringify( "" cesmi/usr-ltl.cpp )

set( GENERATED_INSTANCES_DIR "${CMAKE_CURRENT_BINARY_DIR}/instances/auto" )
add_executable( instances-generator instances/generator/generator.cpp )
file( MAKE_DIRECTORY ${GENERATED_INSTANCES_DIR} )
add_custom_command( TARGET instances-generator POST_BUILD
    COMMAND "${CMAKE_CURRENT_BINARY_DIR}/instances-generator" ${MAX_INSTANCE_FILES_COUNT} ${MIN_INSTANCES_PER_FILE}
    WORKING_DIRECTORY ${GENERATED_INSTANCES_DIR}
    COMMENT "Generating instance files"
    VERBATIM )
target_link_libraries( instances-generator wibble )
target_link_libraries( instances-generator libdivine )

include_directories( ${DIVINE_INCLUDES} )
add_definitions( ${MPI_COMPILE_FLAGS} ${LLVM_COMPILE_FLAGS} )
set_source_files_properties( ${SRC_LLVM} PROPERTIES COMPILE_FLAGS "-fno-rtti" )
set_source_files_properties( ${LLVM_TS_OUTPUTS} llvm/constants.cpp PROPERTIES COMPILE_FLAGS "-fno-rtti" )

add_library( libdivine STATIC
    ${SRC_DLFCN} ${SRC} ${SRC_TIMED}
    ${SRC_COIN} ${SRC_BA} ${SRC_LLVM} ${SRC_DVE}
    ${LLVM_TS_OUTPUTS}
    "${VERSION_FILE}" "${VERSION_FILE}.check" "flags-generated.cpp"
    ${STRINGIFIED} )

add_library( libdivine-instances OBJECT ${SRC_instances} ${SRC_auto_instances} )
add_dependencies( libdivine-instances instances-generator )

set_target_properties( libdivine PROPERTIES OUTPUT_NAME "divine" )

wibble_add_test( divine-test ${testh} ${llvmtesth} )

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

target_link_libraries( libdivine wibble )
target_link_libraries( divine-test libdivine )

if( NOT WIN32 )
  target_link_libraries( libdivine dl )
  wibble_check_target( divine-test )
else()
    target_link_libraries( libdivine psapi )
endif()

install( TARGETS libdivine DESTINATION lib COMPONENT divine_dev )

install( FILES ${H} DESTINATION include/divine COMPONENT divine_dev )
install( FILES ${H_BA} DESTINATION include/divine/ltl2ba COMPONENT divine_dev )
