#add_subdirectory( native )

include( stringify )

file( GLOB H_RUNTIME LIST_DIRECTORIES false
      *.h divine/*.h dios/*.hpp dios/core/*.hpp dios/core/*.def atomic sys/*.h bits/*.h
      abstract/*.hpp abstract/*.h
      pdclib/*.h pdclib/sys/*.h pdclib/bits/*.h libtre/*.h
      libm/*.h libm/machine/*.h
      libcxxabi/include/*
      libcxxabi/src/*.h libcxxabi/src/*.hpp libcxxabi/src/*.ipp
      libcxx/include/* libcxx/ext/* libcxx/include/experimental/*
      libcxx/src/*.h
      libcxx/include/support/xlocale/xlocale.h libcxx/src/support/atomic_support.h
      libunwind/include/* libunwind/include/mach-o/* dios/filesystem/*.h )

file( GLOB SRC_libdios
      divine/*.cpp dios/*.cpp dios/core/*.cpp dios/filesystem/*.cpp )

file( GLOB SRC_libabstract abstract/*.cpp abstract/*.c )

file( GLOB SRC_libc
      pdclib/functions/*/*.c pdclib/opt/*/*.c pdclib/posix/*.c pdclib/*.c libtre/*.c libm/src/*.c )

file( GLOB SRC_libm libm/*.c )
file( GLOB SRC_libcxxabi libcxxabi/src/*.cpp )
file( GLOB SRC_libcxx libcxx/src/*.cpp  )

macro( mklib lib flags )
  foreach( f ${SRC_${lib}} )
    string( REGEX REPLACE "\\.[a-z]+" ".bc" out ${f} )
    file( RELATIVE_PATH out "${CMAKE_CURRENT_SOURCE_DIR}" "${out}" )
    list( APPEND BC_${lib} bc/${out} )
    add_custom_command(
        DEPENDS runtime-cc ${f} ${H_RUNTIME}
        OUTPUT bc/${out}
        COMMAND mkdir -p bc
        COMMAND runtime-cc ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
                           ${f} bc/${out} ${flags} )
    stringify( "runtime" "." ${f} )
  endforeach()

  add_custom_command(
    DEPENDS runtime-ld ${BC_${lib}}
    OUTPUT "bc/${lib}.a"
    COMMAND runtime-ld bc/${lib}.a ${BC_${lib}}
  )

  stringify( "runtime" "${CMAKE_CURRENT_BINARY_DIR}/bc" "${lib}.a" )
endmacro()

list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR} )
list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR}/pdclib )
list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR}/libm )
list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR}/libm/src/ld80 )
list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR}/libtre )
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
mklib( libcxx "${flags};-D_LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE" )
list( APPEND flags -I${CMAKE_CURRENT_SOURCE_DIR}/filesystem -I${CMAKE_CURRENT_BINARY_DIR}
                   -Wall -Wextra -Wold-style-cast -Werror )
mklib( libdios "${flags}" )
mklib( libabstract "${flags}" )

foreach( f ${H_RUNTIME} )
  stringify( "runtime" "." ${f} )
endforeach()

set( OPS_src "${CMAKE_SOURCE_DIR}/llvm/include/llvm/IR/Instruction.def" )
set( OPS_dest "divine/Instruction.def" )
file( COPY ${OPS_src} DESTINATION "divine" )
stringify( "runtime" ${CMAKE_CURRENT_BINARY_DIR} ${OPS_dest} )
stringlist( "runtime" runtime )

include_directories( ${divine_SOURCE_DIR} )
add_definitions( -Wno-overlength-strings ${DIVINE_DEFINES} )
add_library( divine-rt ${divine_SOURCE_DIR}/divine/rt/runtime.cpp
                       ${runtime_FILES} runtime_list.cpp )
set_target_properties( divine-rt PROPERTIES POSITION_INDEPENDENT_CODE ON )
install( TARGETS divine-rt DESTINATION lib )
