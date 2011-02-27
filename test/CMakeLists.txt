file( GLOB tests ${CMAKE_SOURCE_DIR}/test/*.sh )
add_custom_target( functional
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/*.dve .
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/*.m .
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/*.coin .
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/*.mdve .
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/*.ltl .
  COMMAND ${WIBBLE_WRAP_TESTS} sh ${CMAKE_SOURCE_DIR}/test/harness "${divine_BINARY_DIR}/tools" "${MPIEXEC}" ${tests}
)

add_dependencies( functional examples )
