with_warnings()

include_directories( ${CMAKE_CURRENT_BINARY_DIR} ${DIVINE_INCLUDES} )
include_directories( SYSTEM ${DIVINE_SYS_INCLUDES} )
add_definitions( ${MPI_COMPILE_FLAGS} ${LLVM_COMPILE_FLAGS} )

add_executable( divine divine.cpp compile.cpp )

if ( GEN_LLVM )
  set_source_files_properties( lart.cpp PROPERTIES COMPILE_FLAGS -fno-rtti )
  add_executable( lart lart.cpp )
  target_link_libraries( lart liblart ${LLVM_LIBRARIES} ${LLVM_SYSLIBS} )
endif()

target_link_libraries( divine ${LIBDIVINE_INSTANCES} libdivine )

if( NOT WIN32 )
  target_link_libraries( divine pthread )
endif( NOT WIN32 )

if( OPT_HOARD )
  target_link_libraries( divine hoard )
endif()

if( OPT_TBBMALLOC )
  target_link_libraries( divine tbbmalloc )
endif()

set( llvmpaths "
#pragma once
static const char *_cmd_cc= \"${CMD_CC}\"\;
static const char *_cmd_cxx= \"${CMD_CXX}\"\;
static const char *_cmd_clang= \"${CMD_CLANG}\"\;" )

set( llvmpaths_file "${CMAKE_CURRENT_BINARY_DIR}/llvmpaths.h" )
update_file( "${llvmpaths_file}" "${llvmpaths}" )

set_source_files_properties( compile.h PROPERTIES OBJECT_DEPENDS ${llvmpaths_file} )

set( MAKE_STRING_CONST "${CMAKE_CURRENT_SOURCE_DIR}/make-string-const.pl" )

# target_link_libraries( divine lpsolve )
if( ${CMAKE_SYSTEM_NAME} STREQUAL "Linux" )
    target_link_libraries( divine rt )
endif()

if( OPT_LTL2DSTAR )
  target_link_libraries( divine ltl2dstar )
endif( OPT_LTL2DSTAR )

if( WIN32 )
  set( INSTALL_BIN_DEST ./ )
else( WIN32 )
  set( INSTALL_BIN_DEST bin )
endif( WIN32 )

install( TARGETS divine
         DESTINATION ${INSTALL_BIN_DEST} COMPONENT console_tools )
