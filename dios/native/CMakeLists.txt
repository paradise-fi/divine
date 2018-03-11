include_directories( ${CMAKE_CURRENT_BINARY_DIR} ${DIVINE_INCLUDES} )
include_directories( SYSTEM ${DIVINE_SYS_INCLUDES} )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )
add_definitions( ${DIVINE_DEFINES} -DDIVINE_NATIVE_RUNTIME )

add_library( nativert SHARED vm.cpp )
target_compile_options( nativert PUBLIC "-g" "-UNDEBUG" "-O0" "-fvisibility=hidden" )
target_link_libraries( nativert unwind )
