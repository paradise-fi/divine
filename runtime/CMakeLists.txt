add_subdirectory( native )

include( stringify )

file( GLOB SRC_RUNTIME LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
      *.cpp divine/*.cpp divine/dios/*.cpp
      *.h divine/*.h divine/dios/*.h atomic sys/*.h bits/*.h
      pdclib/*.h pdclib/sys/*.h pdclib/bits/*.h pdclib/functions/*/*.c
      pdclib/opt/*/*.c pdclib/posix/*.c pdclib/*.c
      libm/*.c libm/*.h libm/bits/*.h
      libcxxabi/src/* libcxxabi/include/*
      libcxx/src/*.cpp libcxx/src/*.h libcxx/include/* libcxx/ext/* libcxx/include/experimental/*
      libcxx/include/support/xlocale/xlocale.h libcxx/src/support/atomic_support.h
      libunwind/include/* libunwind/src/* libunwind/include/mach-o/*
      filesystem/*.cpp filesystem/*.h )
foreach( f ${SRC_RUNTIME} )
    stringify( "runtime" ${f} )
endforeach()

set( OPS_src "${CMAKE_SOURCE_DIR}/llvm/include/llvm/IR/Instruction.def" )
set( OPS_dest "divine/Instruction.def" )
file( COPY ${OPS_src} DESTINATION "divine" )
stringifyInDir( "runtime" ${CMAKE_CURRENT_BINARY_DIR} ${OPS_dest} )
stringlist( "runtime" runtime )

add_library( libdivinert_str STATIC ${STRINGIFIED} runtime_list.cpp )
if( NOT WIN32 )
    set_target_properties( libdivinert_str PROPERTIES POSITION_INDEPENDENT_CODE ON )
endif()
