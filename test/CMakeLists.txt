set( root ${CMAKE_SOURCE_DIR}/test )
file( GLOB tests ${root}/algorithm/*.sh ${root}/mpi/*.sh ${root}/reduction/*.sh ${root}/tools/*.sh )
add_custom_target( functional
  COMMAND cp -R ${CMAKE_SOURCE_DIR}/test/data .
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/lib .
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/check .
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/run .
  COMMAND ${WIBBLE_WRAP_TESTS} sh ${CMAKE_SOURCE_DIR}/test/harness
    "TOOLS='${divine_BINARY_DIR}/tools'"
    "MPIEXEC='${MPIEXEC}'"
    "WIN32='${WIN32}'"
    "O_DVE='${DVE}'"
    "O_POOLS='${POOLS}'"
    "O_LEGACY='${LEGACY}'"
    "O_LLVM='${LLVM}'" ${tests}
)

add_dependencies( functional examples )
