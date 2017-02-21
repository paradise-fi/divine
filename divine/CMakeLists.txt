include( stringify )
#include( template-separe )

foreach( D vm ss mc ui cc ltl )
    file( GLOB CPP_${D} ${D}/*.cpp )
    file( GLOB HPP_${D} ${D}/*.hpp )
    install( FILES ${HPP_${D}} DESTINATION include/divine/${D} )
endforeach()

#template_separe( VM vm/interpreter.cpp )

set( VERSION_FILE ${divine_BINARY_DIR}/divine/version-generated.cpp )
set( UPDATE_VERSION ${divine_SOURCE_DIR}/releng/update-version-sha.sh )

add_custom_command(
  COMMAND sh ${UPDATE_VERSION} "${SHA1SUM}" "${divine_SOURCE_DIR}" "${VERSION_FILE}" "${DIVINE_VERSION}" "${CMAKE_BUILD_TYPE}"
  DEPENDS ${UPDATE_VERSION}
  VERBATIM
  OUTPUT "${VERSION_FILE}.check"
)
set_source_files_properties( "${VERSION_FILE}.check" PROPERTIES SYMBOLIC ON )
set_source_files_properties( "${VERSION_FILE}" PROPERTIES GENERATED ON )

add_custom_command(
  OUTPUT flags-generated.cpp
  VERBATIM
  COMMAND sh -c "echo \"const char* divineCompileFlags = \\\"${BUILDOPTS}\\\";\">flags-generated.cpp"
)

include_directories( ${DIVINE_INCLUDES} ${CMAKE_BINARY_DIR} )
include_directories( SYSTEM ${DIVINE_SYS_INCLUDES} )
add_definitions( ${MPI_COMPILE_FLAGS} ${LLVM_COMPILE_FLAGS} )
add_definitions( ${DIVINE_DEFINES} )

stringify( doc ${divine_SOURCE_DIR}/doc manual/sim.md )

add_library( divine-cc ${CPP_cc} )
add_library( divine-ltl ${CPP_ltl} )
# add_library( divine-ss ${CPP_ss} )
# add_library( divine-mc ${CPP_mc} )
add_library( divine-ui ${CPP_ui} ${doc_FILES}
                       ${VERSION_FILE} ${VERSION_FILE}.check
                       "flags-generated.cpp" )
add_library( divine-vm ${CPP_vm} )

if ( OPT_SIM )
  target_link_libraries( divine-ui edit )
endif()

# target_compile_features( libdivine PUBLIC cxx_relaxed_constexpr )

bricks_unittest( test-divine ${HPP_ss} ${HPP_vm} ${HPP_mc} ${HPP_cc} ${HPP_ltl} )

foreach( D vm ui cc ltl )
  set_target_properties( divine-${D} PROPERTIES POSITION_INDEPENDENT_CODE ON )
  install( TARGETS divine-${D} DESTINATION lib )
endforeach()

target_link_libraries( divine-cc LLVMCore LLVMSupport LLVMMC LLVMIRReader
                                 LLVMBitReader LLVMBitWriter LLVMLinker
                                 LLVMObject clang clangBasic clangCodeGen )
target_link_libraries( divine-vm liblart divine-rt )
target_link_libraries( divine-vm LLVMBitReader LLVMBitWriter LLVMLinker )
target_link_libraries( divine-ui divine-rt divine-cc divine-vm )

if ( OPT_SQL )
    target_link_libraries( divine-ui nanodbc )
endif()

target_link_libraries( test-divine divine-cc divine-vm divine-ltl )

if( WIN32 )
  target_link_libraries( libdivine psapi )
endif()

add_custom_target( unit_divine
    COMMAND sh ${TEST_WRAPPER} ${WITH_LCOV} ${CMAKE_CURRENT_BINARY_DIR}/test-divine
    VERBATIM
    USES_TERMINAL
    DEPENDS test-divine )
add_dependencies( unit unit_divine )

install( TARGETS divine-ui divine-vm divine-cc divine-ltl DESTINATION lib )
install( FILES ${HPP} DESTINATION include/divine )
