file( GLOB tests ${CMAKE_SOURCE_DIR}/test/*.sh )
add_custom_target( functional )
add_custom_target( do-functional
  COMMAND cp ${CMAKE_SOURCE_DIR}/test/*.dve .
  COMMAND sh ${CMAKE_SOURCE_DIR}/test/harness ${divine_BINARY_DIR}/tools ${tests}
)
add_dependencies( do-functional divine-mc )
add_dependencies( functional do-functional )
if( PROFILE )
  lcov_collect( "2" )
  add_dependencies( functional lcov-collect-2 )
  add_dependencies( lcov-collect-2 do-functional )
endif( PROFILE )

add_dependencies( do-functional examples )
