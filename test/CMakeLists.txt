set( root ${CMAKE_SOURCE_DIR}/test )

set( EXPORT_OPTIONS "" )
foreach( opt ${OPTIONS} )
    set( EXPORT_OPTIONS "${EXPORT_OPTIONS}
 export ${opt}='${${opt}}'" )
endforeach()

file( WRITE "${CMAKE_CURRENT_BINARY_DIR}/environment" "
 export TOOLS='${divine_BINARY_DIR}/tools'
 export MPIEXEC='${MPIEXEC}'
 export WIN32='${WIN32}'
 export LLVMCONFIG='${LLVM_CONFIG_EXECUTABLE}'
 export TESTS='${root}'
 export SRC_ROOT='${CMAKE_SOURCE_DIR}'
 ${EXPORT_OPTIONS}" )

set( EXCLUDE_TAGS big )

if ( ${CMAKE_BUILD_TYPE} STREQUAL "Debug" )
    set( EXCLUDE_TAGS big,ext )
endif()

add_custom_target( functional
  COMMAND rm -rf examples lib
  COMMAND cp -R ${CMAKE_SOURCE_DIR}/doc/examples .
  COMMAND cp -R ${CMAKE_SOURCE_DIR}/test/lib .
  COMMAND mkdir -p results
  COMMAND env SRCDIR=${divine_SOURCE_DIR}
              BINDIR=${divine_BINARY_DIR}
              sh ${CMAKE_CURRENT_SOURCE_DIR}/lib/testsuite
                 --testdir ${CMAKE_CURRENT_SOURCE_DIR}
                 --exclude-tags ${EXCLUDE_TAGS}
  USES_TERMINAL
  DEPENDS runner
  VERBATIM
)

add_executable( runner lib/runner.cpp )
add_dependencies( functional divine llvm-utils divcc dioscc )
