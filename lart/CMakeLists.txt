file( GLOB SRC reduction/*.cpp analysis/*.cpp support/*.cpp aa/*.cpp
               interference/*.cpp weakmem/*.cpp abstract/*.cpp
               abstract/domains/*.cpp
               svcomp/*.cpp divine/*.cpp )

file( GLOB H_ABSTRACT abstract/*.h )
install( FILES ${H_ABSTRACT} DESTINATION include/lart/abstract )

include_directories( ${DIVINE_INCLUDES} )
add_definitions( ${DIVINE_DEFINES} )

add_library( liblart ${SRC} )
set_target_properties( liblart PROPERTIES OUTPUT_NAME "lart"
                                          POSITION_INDEPENDENT_CODE ON )
install( TARGETS liblart DESTINATION lib )

bricks_unittest( test-lart ${H_ABSTRACT} )

target_link_libraries( liblart LLVMCore LLVMSupport LLVMMC LLVMIRReader
                               LLVMBitWriter LLVMCodeGen LLVMLinker )
target_link_libraries( test-lart divine-rt divine-cc liblart )

add_custom_target( unit_lart
    COMMAND sh ${TEST_WRAPPER} ${WITH_LCOV} ${CMAKE_CURRENT_BINARY_DIR}/test-lart
    VERBATIM
    USES_TERMINAL
    DEPENDS test-lart )
add_dependencies( unit unit_lart )
