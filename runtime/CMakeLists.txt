#add_subdirectory( native )

include( stringify )

file( GLOB H_RUNTIME LIST_DIRECTORIES false
      *.h divine/*.h dios/*.hpp dios/*.def atomic sys/*.h bits/*.h
      pdclib/*.h pdclib/sys/*.h pdclib/bits/*.h
      libm/*.h libm/bits/*.h
      libcxxabi/include/*
      libcxxabi/src/*.h libcxxabi/src/*.hpp libcxxabi/src/*.ipp
      libcxx/include/* libcxx/ext/* libcxx/include/experimental/*
      libcxx/src/*.h
      libcxx/include/support/xlocale/xlocale.h libcxx/src/support/atomic_support.h
      libunwind/include/* libunwind/include/mach-o/* filesystem/*.h )

file( GLOB SRC_libdios
      divine/*.cpp dios/*.cpp filesystem/*.cpp )

file( GLOB SRC_libc
      pdclib/functions/*/*.c pdclib/opt/*/*.c pdclib/posix/*.c pdclib/*.c )

file( GLOB SRC_libm libm/*.c )
file( GLOB SRC_libcxxabi libcxxabi/src/*.cpp )
file( GLOB SRC_libcxx libcxx/src/*.cpp  )

macro( mklib lib flags )
  foreach( f ${SRC_${lib}} )
    string( REGEX REPLACE "\\.[a-z]+" ".bc" out ${f} )
    file( RELATIVE_PATH out "${CMAKE_CURRENT_SOURCE_DIR}" "${out}" )
    list( APPEND BC_${lib} ${out} )
    add_custom_command(
        DEPENDS runtime-cc ${f} ${H_RUNTIME}
        OUTPUT ${out}
        COMMAND runtime-cc ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
                           ${f} ${out} ${flags} )
    stringify( "runtime" ${f} )
  endforeach()

  add_custom_command(
    DEPENDS runtime-ld ${BC_${lib}}
    OUTPUT "${lib}.bc"
    COMMAND runtime-ld ${lib}.bc ${BC_${lib}}
  )

  stringifyInDir( "runtime" "${CMAKE_CURRENT_BINARY_DIR}" "${lib}.bc" )
endmacro()

list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR} )
list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR}/pdclib )
list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR}/libm )
list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR}/libunwind/include )
list( APPEND flags -Oz )
list( APPEND flags -g )
list( APPEND flags -D__divine__ )
list( APPEND flags -D_POSIX_C_SOURCE=2008098L )
list( APPEND flags -D_LITTLE_ENDIAN=1234 )
list( APPEND flags -D_BYTE_ORDER=1234 )

mklib( libc "${flags};-D_PDCLIB_BUILD" )

list( APPEND flags -std=c++14 )
list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR}/libcxx/include )
list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR}/libcxxabi/include )
list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR}/libcxxabi/src )

mklib( libcxxabi "${flags};-DLIBCXXABI_USE_LLVM_UNWINDER" )
mklib( libcxx "${flags}" )
list( APPEND flags -I${CMAKE_CURRENT_SOURCE_DIR}/filesystem )
mklib( libdios "${flags};-I${CMAKE_CURRENT_BINARY_DIR}" )

foreach( f ${H_RUNTIME} )
  stringify( "runtime" ${f} )
endforeach()

set( OPS_src "${CMAKE_SOURCE_DIR}/llvm/include/llvm/IR/Instruction.def" )
set( OPS_dest "divine/Instruction.def" )
file( COPY ${OPS_src} DESTINATION "divine" )
stringifyInDir( "runtime" "${CMAKE_CURRENT_BINARY_DIR}" ${OPS_dest} )
stringlist( "runtime" runtime )

include_directories( ${divine_SOURCE_DIR} )
add_definitions( -Wno-overlength-strings )
add_library( divine-rt ${divine_SOURCE_DIR}/divine/rt/runtime.cpp
                       ${STRINGIFIED} runtime_list.cpp
                       ${CMAKE_CURRENT_BINARY_DIR}/libc_bc_str.cpp )
