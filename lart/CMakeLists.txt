file( GLOB SRC reduction/*.cpp analysis/*.cpp support/*.cpp aa/*.cpp
               interference/*.cpp weakmem/*.cpp abstract/*.cpp svcomp/*.cpp
               divine/*.cpp )

file( GLOB H_ABSTRACT abstract/*.h )
install( FILES ${H_ABSTRACT} DESTINATION include/lart/abstract )

include_directories( ${DIVINE_INCLUDES} )
add_definitions( ${DIVINE_DEFINES} )

add_library( liblart ${SRC} )
set_target_properties( liblart PROPERTIES OUTPUT_NAME "lart"
                                          POSITION_INDEPENDENT_CODE ON )

bricks_unittest( test-lart ${H_ABSTRACT} )

target_link_libraries( test-lart libdivine )

add_custom_target( unit_lart
    COMMAND sh ${TEST_WRAPPER} ${WITH_LCOV} ${CMAKE_CURRENT_BINARY_DIR}/test-lart
    VERBATIM
    USES_TERMINAL
    DEPENDS test-lart )
add_dependencies( unit unit_lart )
