add_definitions( -Wall -Wno-unused-variable -Wno-unused-function -Wno-overloaded-virtual )
add_definitions( -Wextra -Wno-unused-parameter -Wno-sign-compare )

include_directories( ${DIVINE_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR} )
add_definitions( ${MPI_COMPILE_FLAGS} ${LLVM_COMPILE_FLAGS} )

link_libraries( libdivine libdivine-instances wibble )
if( MURPHI )
    link_libraries( murphi )
endif()

if( NOT WIN32 )
  link_libraries( libdivine pthread )
endif( NOT WIN32 )

if( HOARD )
  link_libraries( hoard )
endif( HOARD )

if( TBBMALLOC )
  link_libraries( tbbmalloc )
endif()

set( llvmpaths "${CMAKE_CURRENT_BINARY_DIR}/llvmpaths.h" )
file( WRITE ${llvmpaths} "static const char *_clang_cmd = \"${CLANG_CMD}\";\n" )
file( APPEND ${llvmpaths} "static const char *_gold_cmd = \"${GOLD_CMD}\";\n" )
file( APPEND ${llvmpaths} "static const char *_ar_cmd= \"${AR_CMD}\";\n" )
file( APPEND ${llvmpaths} "static const char *_llvmgold_path= \"${LLVMGOLD_PATH}\";\n" )

set_source_files_properties( compile.h PROPERTIES OBJECT_DEPENDS ${llvmpaths} )

set( MAKE_STRING_CONST "${CMAKE_CURRENT_SOURCE_DIR}/make-string-const.pl" )

set( COMBINE_M4_H "${CMAKE_CURRENT_BINARY_DIR}/combine.m4.h" )
set( COMBINE_M4 "${CMAKE_CURRENT_SOURCE_DIR}/combine.m4" )

add_executable( divine divine.cpp compile.cpp combine.cpp ${COMBINE_M4_H} )
# target_link_libraries( divine lpsolve )
if( NOT WIN32 )
    target_link_libraries( divine rt )
endif()

add_executable( divine.simple EXCLUDE_FROM_ALL simple.cpp )

if( LTL2DSTAR )
  target_link_libraries( divine ltl2dstar )
endif( LTL2DSTAR )

if( WIN32 )
  set( INSTALL_BIN_DEST ./ )
else( WIN32 )
  set( INSTALL_BIN_DEST bin )
endif( WIN32 )

install( TARGETS divine
         DESTINATION ${INSTALL_BIN_DEST} COMPONENT console_tools )

add_custom_command(
  OUTPUT ${COMBINE_M4_H}
  DEPENDS ${COMBINE_M4} ${MAKE_STRING_CONST}
  COMMAND ${PERL_EXECUTABLE} ${MAKE_STRING_CONST} "const char *combine_m4 = " ${COMBINE_M4} > ${COMBINE_M4_H} )

