file( GLOB SRC reduction/*.cpp analysis/*.cpp support/*.cpp aa/*.cpp
               interference/*.cpp weakmem/*.cpp abstract/*.cpp svcomp/*.cpp
               divine/*.cpp )

include_directories( ${DIVINE_INCLUDES} )
add_definitions( ${DIVINE_DEFINES} )

add_library( liblart ${SRC} )
set_target_properties( liblart PROPERTIES OUTPUT_NAME "lart"
                                          POSITION_INDEPENDENT_CODE ON )
