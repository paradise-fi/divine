include( stringify )
#include( template-separe )

foreach( D mem vm ss mc ui cc ltl sim smt dbg )
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
  BYPRODUCTS "${VERSION_FILE}"
  OUTPUT "${VERSION_FILE}.check" "${VERSION_FILE}"
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

# add_library( divine-ss ${CPP_ss} )
add_library( divine-ui ${CPP_ui} ${VERSION_FILE} ${VERSION_FILE}.check "flags-generated.cpp" )
foreach( D cc ltl mc smt vm dbg )
  add_library( divine-${D} ${CPP_${D}} )
endforeach()

if ( OPT_SIM )
  stringify( doc ${divine_SOURCE_DIR}/doc manual/sim.md )
  add_library( divine-sim ${CPP_sim} ${doc_FILES} )
  target_link_libraries( divine-sim ${LIBEDIT} ${LIBTINFO} divine-dbg )
  target_link_libraries( divine-ui divine-sim )
endif()

# target_compile_features( libdivine PUBLIC cxx_relaxed_constexpr )

bricks_unittest( test-divine ${HPP_ss} ${HPP_mem} ${HPP_vm} ${HPP_mc} ${HPP_cc} ${HPP_ltl} ${HPP_smt} )

foreach( D vm ui cc ltl smt )
  set_target_properties( divine-${D} PROPERTIES POSITION_INDEPENDENT_CODE ON )
  install( TARGETS divine-${D} DESTINATION lib )
endforeach()

target_link_libraries( divine-cc LLVMCore LLVMSupport LLVMMC LLVMIRReader
                                 LLVMBitReader LLVMBitWriter LLVMLinker
                                 LLVMObject clang clangBasic clangCodeGen )
target_link_libraries( divine-smt ${Z3_LIBRARIES} )
target_link_libraries( divine-dbg divine-vm )
target_link_libraries( divine-mc divine-vm divine-dbg divine-smt divine-rt divine-cc # FIXME divine-cc
                                 liblart LLVMBitReader LLVMBitWriter LLVMLinker )
target_link_libraries( divine-ui divine-rt divine-cc divine-mc )

if ( OPT_SQL )
    target_link_libraries( divine-ui nanodbc )
endif()

target_link_libraries( test-divine divine-cc divine-vm divine-ltl divine-dbg divine-mc )

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
