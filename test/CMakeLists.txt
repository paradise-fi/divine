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
 ${EXPORT_OPTIONS}" )

add_custom_target( functional
  COMMAND cp -R ${CMAKE_SOURCE_DIR}/test/data .
  COMMAND rm -rf llvm_examples
  COMMAND cp -R ${CMAKE_SOURCE_DIR}/examples/llvm llvm_examples
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/lib .
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/flavour .
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/util .
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/check .
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/run .
  COMMAND mkdir -p results
  COMMAND sh ${TEST_WRAPPER} ${WITH_LCOV} ./runner --flavours vanilla,part+none,shared+none,part+tree,shared+tree --total-timeout 28800 --timeout 600 --testdir ${CMAKE_CURRENT_SOURCE_DIR} --workdir . --outdir results
  USES_TERMINAL
  DEPENDS runner
)

add_executable( runner runner.cpp )
add_dependencies( functional divine )
