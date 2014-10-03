link_libraries( ${LLVM_LIBRARIES} )

add_library( liblart STATIC aa/pass.cpp aa/andersen.cpp abstract/pass.cpp abstract/va.cpp abstract/bcp.cpp )
set_target_properties( liblart PROPERTIES OUTPUT_NAME "lart" )
