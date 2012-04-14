set( root ${CMAKE_SOURCE_DIR}/test )
file( GLOB tests ${root}/algorithm/*.sh ${root}/mpi/*.sh )
add_custom_target( functional
  COMMAND cp -R ${CMAKE_SOURCE_DIR}/test/data .
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/lib .
  COMMAND ${WIBBLE_WRAP_TESTS} sh ${CMAKE_SOURCE_DIR}/test/harness "${divine_BINARY_DIR}/tools" "${MPIEXEC}" ${tests}
)

add_dependencies( functional examples )
