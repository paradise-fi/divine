file( GLOB tests ${CMAKE_SOURCE_DIR}/test/*.sh )
add_custom_target( functional 
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/*.dve .
  COMMAND sh ${CMAKE_SOURCE_DIR}/test/harness ${tests}
)
add_dependencies( functional divine-mc )
add_dependencies( check functional )
