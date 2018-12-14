list( APPEND flags -Oz )
list( APPEND flags -debug-info-kind=standalone )
list( APPEND flags -D__divine__ )
list( APPEND flags -D_POSIX_C_SOURCE=2008098L )
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
list( APPEND flags -isystem${divine_SOURCE_DIR}/ ) # for #includes starting with dios/
list( APPEND flags -isystem${divine_SOURCE_DIR}/bricks )

set( NOEXCEPT "-fno-rtti;-fno-exceptions" )
mkobjs( libc "${flags};-D_PDCLIB_BUILD" )
mkobjs( libc_cpp "${flags};-D_PDCLIB_BUILD;${NOEXCEPT};-std=c++1z;-I${CMAKE_CURRENT_BINARY_DIR}" )
mkobjs( libpthread "${flags};${NOEXCEPT};-std=c++1z" )
mkobjs( libm "${flags}" )

list( APPEND flags -std=c++1z )
mkobjs( libcxxabi "${flags};-DLIBCXXABI_USE_LLVM_UNWINDER" )
mkobjs( libcxx    "${flags};-D_LIBCPP_BUILDING_LIBRARY;-DLIBCXX_BUILDING_LIBCXXABI" )

list( APPEND flags -I${CMAKE_CURRENT_SOURCE_DIR}/fs -I${CMAKE_CURRENT_BINARY_DIR}
                   -Wall -Wextra -Wold-style-cast -Werror)
mkobjs( dios "${flags};-D__dios_kernel__;${NOEXCEPT}" )
mkobjs( librst "${flags};${NOEXCEPT}" )

mklib( libc libc_cpp )
mklib( libm )
mklib( libcxxabi )
mklib( libcxx)
mklib( dios )
mklib( librst )
mklib( libpthread )

foreach( f ${H_RUNTIME} )
  stringify( "dios" "." ${f} )
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

# vim: ft=cmake
