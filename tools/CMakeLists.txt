with_warnings()

include_directories( ${CMAKE_CURRENT_BINARY_DIR} ${DIVINE_INCLUDES} )
include_directories( SYSTEM ${DIVINE_SYS_INCLUDES} )
add_definitions( ${LLVM_COMPILE_FLAGS} )
add_definitions( ${DIVINE_DEFINES} )

add_executable( divine divine.cpp )

if ( GEN_LLVM )
  set_source_files_properties( lart.cpp PROPERTIES COMPILE_FLAGS -fno-rtti )
  add_executable( lart lart.cpp )
  target_link_libraries( lart liblart ${LLVM_LIBRARIES} ${LLVM_SYSLIBS} )
endif()

if ( GEN_LLVM )
    add_executable( divinecc cc.cpp )
    target_link_libraries( divinecc libdivine liblart ${LLVM_LIBRARIES} 
        clangCodeGen
        clangFrontend
        clangSerialization
        clangDriver
        clangParse
        clangSema
        clangAnalysis
        clangAST
        clangBasic
        clangEdit
        clangLex
        clangTooling
        ${LLVM_SYSLIBS} )
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
