set( root ${CMAKE_SOURCE_DIR}/test )

file( GLOB tests ${root}/algorithm/*.sh
                 ${root}/mpi/*.sh
                 ${root}/shared/*.sh
                 ${root}/reduction/*.sh
                 ${root}/tools/*.sh
                 ${root}/llvm/*.sh )

add_custom_target( functional
  COMMAND cp -R ${CMAKE_SOURCE_DIR}/test/data .
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/lib .
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/util .
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/check .
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/run .
  COMMAND ${WIBBLE_WRAP_TESTS} sh ${CMAKE_SOURCE_DIR}/test/harness
    "TOOLS='${divine_BINARY_DIR}/tools'"
    "MPIEXEC='${MPIEXEC}'"
    "WIN32='${WIN32}'"
    "O_DVE='${DVE}'"
    "O_LTL3BA='${LTL3BA}'"
    "O_MPI='${MPI}'"
    "O_POOLS='${POOLS}'"
    "O_LEGACY='${LEGACY}'"
    "O_TIMED='${TIMED}'"
    "O_COMPRESSION='${COMPRESSION}'"
    "O_MURPHI='${MURPHI}'"
    "LLVMCONFIG='${LLVM_CONFIG_EXECUTABLE}'"
    "O_LLVM='${LLVM}'" ${tests}
)

add_dependencies( functional examples )
add_dependencies( functional divine )
