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

set( ONLY --only "/[1-4][.][^/]+$" )

if ( ${CMAKE_BUILD_TYPE} STREQUAL "Debug" )
    set( ONLY --only "/[1-3][.][^/]+$" )
endif()

add_custom_target( functional
  COMMAND rm -rf examples lib
  COMMAND cp -R ${CMAKE_SOURCE_DIR}/doc/examples .
  COMMAND cp -R ${CMAKE_SOURCE_DIR}/test/lib .
  COMMAND mkdir -p results
  COMMAND ${WITH_LCOV}
            bash ${CMAKE_CURRENT_SOURCE_DIR}/lib/testsuite
                 ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
                 ${CMAKE_CURRENT_BINARY_DIR} ${ONLY}
  USES_TERMINAL
  DEPENDS runner
  VERBATIM
)

add_executable( runner lib/runner.cpp )
add_dependencies( functional divine llvm-utils )
