include_directories( ${CMAKE_CURRENT_BINARY_DIR} ${DIVINE_INCLUDES} )
include_directories( SYSTEM ${DIVINE_SYS_INCLUDES} )
add_definitions( ${LLVM_COMPILE_FLAGS} )
add_definitions( ${DIVINE_DEFINES} )

add_executable( runtime-cc runtime-cc.cpp )
add_executable( runtime-ld runtime-ld.cpp )
add_executable( divcc divcc.cpp )

target_link_libraries( runtime-cc divine-cc )
target_link_libraries( runtime-ld divine-cc )
target_link_libraries( divcc divine-cc divine-rt divine-ui )
target_link_libraries( divcc LLVMTarget LLVMAsmPrinter )
target_link_libraries( divcc lldELF )

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
  set( BENCH bench-common.cpp bench-report.cpp bench-run.cpp )
  add_executable( divbench ${BENCH} divbench.cpp )
  add_executable( extbench ${BENCH} bench-external.cpp extbench.cpp )
  target_link_libraries( divbench divine-ui divine-rt pthread )
  target_link_libraries( extbench divine-ui pthread ) # FIXME divine-ui
endif()

add_executable( divcheck divcheck.cpp )
target_link_libraries( divcheck divine-ui divine-rt )

if( NOT WIN32 )
  target_link_libraries( divine pthread )
  target_link_libraries( divcheck pthread )
endif( NOT WIN32 )

if( OPT_TBBMALLOC )
  target_link_libraries( divine tbbmalloc )
endif()

set( MAKE_STRING_CONST "${CMAKE_CURRENT_SOURCE_DIR}/make-string-const.pl" )

# target_link_libraries( divine lpsolve )
if( ${CMAKE_SYSTEM_NAME} STREQUAL "Linux" )
    target_link_libraries( divine rt )
endif()

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
