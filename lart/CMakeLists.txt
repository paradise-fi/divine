link_libraries( ${LLVM_LIBRARIES} )

include_directories( ${DIVINE_INCLUDES} )
add_definitions( ${LLVM_COMPILE_FLAGS} )
file( GLOB SRC reduction/*.cpp analysis/*.cpp support/*.cpp aa/*.cpp interference/*.cpp weakmem/*.cpp abstract/*.cpp svcomp/*.cpp )
add_library( liblart STATIC ${SRC} )
set_target_properties( liblart PROPERTIES OUTPUT_NAME "lart" )
