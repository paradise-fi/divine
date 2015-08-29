link_libraries( ${LLVM_LIBRARIES} )

include_directories( ${DIVINE_INCLUDES} )
add_definitions( ${LLVM_COMPILE_FLAGS} )
add_library( liblart STATIC support/id.cpp support/composite.cpp
                            aa/pass.cpp aa/andersen.cpp
                            interference/pass.cpp
                            weakmem/pass.cpp
                            interrupt/pass.cpp
                            abstract/common.cpp abstract/pass.cpp abstract/va.cpp abstract/bcp.cpp )
set_target_properties( liblart PROPERTIES OUTPUT_NAME "lart" )
