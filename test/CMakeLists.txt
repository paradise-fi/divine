set( root ${CMAKE_SOURCE_DIR}/test )

file( WRITE "${CMAKE_CURRENT_BINARY_DIR}/environment" "
 export TOOLS='${divine_BINARY_DIR}/tools'
 export MPIEXEC='${MPIEXEC}'
 export WIN32='${WIN32}'
 export O_DVE='${DVE}'
 export O_LTL3BA='${LTL3BA}'
 export O_MPI='${MPI}'
 export O_POOLS='${POOLS}'
 export O_TIMED='${TIMED}'
 export O_COMPRESSION='${COMPRESSION}'
 export O_MURPHI='${MURPHI}'
 export O_COIN='${COIN}'
 export O_CESMI='${CESMI}'
 export LLVMCONFIG='${LLVM_CONFIG_EXECUTABLE}'
 export O_LLVM='${LLVM}'
 export O_SMALL='${SMALL}'
")

add_custom_target( functional
  COMMAND cp -R ${CMAKE_SOURCE_DIR}/test/data .
  COMMAND rm -rf llvm_examples
  COMMAND cp -R ${CMAKE_SOURCE_DIR}/examples/llvm llvm_examples
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/lib .
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/util .
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/check .
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/run .
  COMMAND ./runner --batch --timeout 180 --testdir ${CMAKE_CURRENT_SOURCE_DIR} --workdir . --outdir results
  DEPENDS runner
)

add_executable( runner runner.cpp )

add_dependencies( functional examples )
add_dependencies( functional divine )
