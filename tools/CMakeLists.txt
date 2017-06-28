include_directories( ${CMAKE_CURRENT_BINARY_DIR} ${DIVINE_INCLUDES} )
include_directories( SYSTEM ${DIVINE_SYS_INCLUDES} )
add_definitions( ${LLVM_COMPILE_FLAGS} )
add_definitions( ${DIVINE_DEFINES} )

add_executable( runtime-cc runtime-cc.cpp )
add_executable( runtime-ld runtime-ld.cpp )
add_executable( divcc divcc.cpp )

target_link_libraries( runtime-cc divine-cc )
target_link_libraries( runtime-ld divine-cc )
target_link_libraries( divcc divine-cc )
target_link_libraries( divcc LLVMTarget )

add_executable( divine divine.cpp )
set_source_files_properties( lart.cpp PROPERTIES COMPILE_FLAGS -fno-rtti )
add_executable( lart lart.cpp  )
target_link_libraries( lart
                       liblart
                       LLVMCore
                       LLVMSupport
                       LLVMIRReader
                       LLVMBitReader
                       LLVMBitWriter
                       LLVMTransformUtils
                       LLVMAnalysis )

target_link_libraries( divine divine-ui divine-rt )

if ( OPT_SQL )
  add_executable( divbench benchmark.cpp bench-report.cpp bench-run.cpp bench-external.cpp )
  target_link_libraries( divbench divine-ui divine-rt pthread )
endif()

add_executable( divcheck divcheck.cpp )
target_link_libraries( divcheck divine-ui divine-rt )

if( NOT WIN32 )
  target_link_libraries( divine pthread )
  target_link_libraries( divcheck pthread )
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

# NB. Must be here (and not in the toplevel cmakelists) to ensure correct
# action ordering during install.
install( SCRIPT ../releng/install-rpath.cmake )
