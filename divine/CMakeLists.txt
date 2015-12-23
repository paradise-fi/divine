with_warnings()

include( stringify )
include( template-separe )

file( GLOB CPP *.cpp )
file( GLOB HPP *.h *.hpp )

foreach( D utility llvm compile toolkit statespace algorithm ui )
    file( GLOB CPP_${D} ${D}/*.cpp )
    file( GLOB HPP_${D} ${D}/*.hpp )
    install( FILES ${HPP_${D}} DESTINATION include/divine/${D} )
endforeach()

stringify( "llvm" llvm/problem.def )
template_separe( LLVM llvm/interpreter.cpp )

set( VERSION_FILE ${divine_BINARY_DIR}/divine/version-generated.cpp )
set( UPDATE_VERSION ${divine_SOURCE_DIR}/divine/update-version.sh )

add_custom_command(
  COMMAND sh ${UPDATE_VERSION} "${SHA1SUM}" "${divine_SOURCE_DIR}" "${VERSION_FILE}" "${DIVINE_VERSION}"
  VERBATIM
  OUTPUT "${VERSION_FILE}.check"
)
set_source_files_properties( "${VERSION_FILE}.check" PROPERTIES SYMBOLIC ON )
set_source_files_properties( "${VERSION_FILE}" PROPERTIES GENERATED ON )

add_custom_command(
  OUTPUT flags-generated.cpp
  VERBATIM
  COMMAND sh -c "echo \"const char* divineCompileFlags = \\\"${BUILDOPTS}\\\";\">flags-generated.cpp"
)

include_directories( ${DIVINE_INCLUDES} ${CMAKE_BINARY_DIR} )
include_directories( SYSTEM ${DIVINE_SYS_INCLUDES} )
add_definitions( ${MPI_COMPILE_FLAGS} ${LLVM_COMPILE_FLAGS} )
add_definitions( ${DIVINE_DEFINES} )

set( llvmpaths "
#pragma once
static const char *_cmd_cc= \"${CMD_CC}\"\;
static const char *_cmd_cxx= \"${CMD_CXX}\"\;
static const char *_cmd_clang= \"${CMD_CLANG}\"\;" )

set( llvmpaths_file "${CMAKE_CURRENT_BINARY_DIR}/llvmpaths.h" )
update_file( "${llvmpaths_file}" "${llvmpaths}" )

set_source_files_properties( compile.h PROPERTIES OBJECT_DEPENDS ${llvmpaths_file} )
add_library( libdivine
  ${CPP_utility} ${CPP_llvm} ${CPP_toolkit} ${CPP_compile}
  ${LLVM_TS_OUTPUTS}
  "${VERSION_FILE}" "${VERSION_FILE}.check" "flags-generated.cpp"
  ${STRINGIFIED} )

target_link_libraries( libdivine pthread )
target_compile_features( libdivine PUBLIC cxx_relaxed_constexpr )
set_target_properties( libdivine PROPERTIES OUTPUT_NAME "divine" )

bricks_unittest( test-divine ${HPP} ${HPP_llvm} ${HPP_ui} ${HPP_statespace} )

if( NOT WIN32 )
  set_target_properties( libdivine PROPERTIES COMPILE_FLAGS -fPIC )
endif()

target_link_libraries( libdivine ${LLVM_LIBRARIES} )
target_link_libraries( libdivine ${LLVM_SYSLIBS} )
target_link_libraries( libdivine pdclib_str )
target_link_libraries( libdivine m_str )
target_link_libraries( libdivine cxxabi_str )
target_link_libraries( libdivine cxx_str )
target_link_libraries( libdivine llvm_usr_str )
target_link_libraries( libdivine ${CURSES_LIBRARIES} )

if ( OPT_SQL )
    target_link_libraries( libdivine nanodbc )
endif()

target_link_libraries( test-divine libdivine )

if( WIN32 )
  target_link_libraries( libdivine psapi )
endif()

add_custom_target( unit_divine
    COMMAND sh ${TEST_WRAPPER} ${WITH_LCOV} ${CMAKE_CURRENT_BINARY_DIR}/test-divine
    VERBATIM
    USES_TERMINAL
    DEPENDS test-divine )
add_dependencies( unit unit_divine )

install( TARGETS libdivine ${LIBDIVINE_INSTANCES} DESTINATION lib )
install( FILES ${HPP} DESTINATION include/divine )
