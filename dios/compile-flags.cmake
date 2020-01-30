list( APPEND flags -Oz )
list( APPEND flags -debug-info-kind=standalone )
list( APPEND flags -D__divine__ )
list( APPEND flags -D__ELF__ )
list( APPEND flags -D__unix__ )
list( APPEND flags -D_POSIX_C_SOURCE=200809L )
list( APPEND flags -D_BSD_SOURCE )
list( APPEND flags -D_LITTLE_ENDIAN=1234 )
list( APPEND flags -D_BYTE_ORDER=1234 )

# C++, also used in the implementation of libc
list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR}/libcxx/include )
list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR}/libcxxabi/include )
list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR}/libcxxabi/src )

list( APPEND flags -isystem${CMAKE_CURRENT_BINARY_DIR}/include ) # dios generated
list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR}/include ) # dios
list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR}/libm/ld80 )
list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR}/libm )
list( APPEND flags -isystem${divine_SOURCE_DIR}/ ) # for #includes starting with dios/
list( APPEND flags -isystem${divine_SOURCE_DIR}/bricks )

set( NOEXCEPT "-fno-rtti;-fno-exceptions" )
set( WARN "-Wall;-Wextra;-Wold-style-cast;-Werror" )

mkobjs( libc "${flags};-D_PDCLIB_BUILD" )
mkobjs( libc_cpp "${flags};-D_PDCLIB_BUILD;${NOEXCEPT};-std=c++17;-I${CMAKE_CURRENT_BINARY_DIR}" )
mkobjs( libpthread "${flags};${NOEXCEPT};-std=c++17" )
mkobjs( libm "${flags}" )

mkobjs( libcxxabi "${flags};-D_LIBCXXABI_BUILDING_LIBRARY;-DLIBCXXABI_USE_LLVM_UNWINDER;-std=c++17" )
mkobjs( libcxx    "${flags};-D_LIBCPP_BUILDING_LIBRARY;-DLIBCXX_BUILDING_LIBCXXABI;-std=c++17" )

list( APPEND flags -I${CMAKE_CURRENT_BINARY_DIR} )
set( flags "${flags};${WARN}" )

if( CMAKE_BUILD_TYPE STREQUAL "Debug" )
  set( EXTRA_OPT "-DNDEBUG" )
else()
  set( EXTRA_OPT "-O3;-DNDEBUG;-D__inline_opt='__attribute__((__always_inline__))'" )
endif()

mkobjs( dios "${flags};-D__dios_kernel__;${NOEXCEPT};-std=c++17" )
mkobjs( config "${flags};-D__dios_kernel__;${NOEXCEPT};-std=c++17" )
mkobjs( librst "${flags};${NOEXCEPT};-std=c++17;${EXTRA_OPT}" )
mkobjs( lamp "${flags};${WARN};${NOEXCEPT};-std=c++17;${EXTRA_OPT}" )

foreach( arch divm klee native )
    mkobjs( dios_${arch} "${flags};${NOEXCEPT}" )
    mklib( dios_${arch} )
endforeach()

mklib( libc libc_cpp )
mklib( libm )
mklib( libcxxabi )
mklib( libcxx )
mklib( dios )
mklib( librst )
mklib( libpthread )

foreach( f ${H_RUNTIME} )
  stringify( "dios" "." ${f} )
endforeach()

foreach( f ${SRC_config} )
  string( REGEX REPLACE ".*/\([a-z]*)\\.[a-z]+" "config/\\1.bc" mod ${f} )
  stringify( "dios" "${CMAKE_CURRENT_BINARY_DIR}/bc" "${mod}" )
endforeach()

foreach( f ${SRC_lamp} )
  string( REGEX REPLACE ".*/\([a-z]*)\\.[a-z]+" "lamp/\\1.bc" mod ${f} )
  stringify( "dios" "${CMAKE_CURRENT_BINARY_DIR}/bc" "${mod}" )
endforeach()

set( OPS_src "${CMAKE_SOURCE_DIR}/llvm/include/llvm/IR/Instruction.def" )
set( OPS_dest "divine/Instruction.def" )
file( COPY ${OPS_src} DESTINATION "divine" )
stringify( "dios" ${CMAKE_CURRENT_BINARY_DIR} ${OPS_dest} )
foreach( hdr divm.h lart.h vmutil.h hostabi.h )
  stringify( "dios" ${CMAKE_CURRENT_BINARY_DIR} include/sys/${hdr} )
endforeach()
stringify( "dios" ${CMAKE_CURRENT_BINARY_DIR} include/stdatomic.h )
stringlist( "dios" dios )
stringlist( "dios_native" dios_native )

# vim: ft=cmake
