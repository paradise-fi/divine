#add_subdirectory( native )

include( stringify )

file( GLOB H_RUNTIME LIST_DIRECTORIES false
      *.h divine/*.h dios/*.hpp dios/core/*.hpp dios/core/*.def atomic sys/*.h bits/*.h
      abstract/*.hpp abstract/*.h
      libc/include/*.h libc/internals/*.h 
      libc/include/sys/*.h libc/include/bits/*.h
      libm/include/*.h libm/include/machine/*.h
      libcxxabi/include/*
      libcxxabi/src/*.h libcxxabi/src/*.hpp libcxxabi/src/*.ipp
      libcxx/include/* libcxx/ext/* libcxx/include/experimental/*
      libcxx/src/*.h
      libcxx/include/support/xlocale/xlocale.h libcxx/src/support/atomic_support.h
      libunwind/include/* libunwind/include/mach-o/* dios/filesystem/*.h )

file( GLOB SRC_libdios
      divine/*.cpp dios/*.cpp dios/core/*.cpp dios/filesystem/*.cpp )

file( GLOB SRC_libabstract abstract/*.cpp abstract/*.c )
file( GLOB SRC_libc libc/functions/*/*.c )
file( GLOB SRC_libm libm/src/*.c )
file( GLOB SRC_libcxxabi libcxxabi/src/*.cpp )
file( GLOB SRC_libcxx libcxx/src/*.cpp  )

set( H_DIVM ${CMAKE_CURRENT_BINARY_DIR}/libc/include/sys/divm.h )

macro( mklib lib flags )
  foreach( f ${SRC_${lib}} )
    string( REGEX REPLACE "\\.[a-z]+" ".bc" out ${f} )
    file( RELATIVE_PATH out "${CMAKE_CURRENT_SOURCE_DIR}" "${out}" )
    list( APPEND BC_${lib} bc/${out} )
    add_custom_command(
        DEPENDS runtime-cc ${f} ${H_RUNTIME} ${H_DIVM}
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

add_custom_command(
    OUTPUT ${H_DIVM}
    VERBATIM
    COMMAND cmake -E copy_if_different ${divine_SOURCE_DIR}/divine/vm/divm.h ${H_DIVM} )

list( APPEND flags -Oz )
list( APPEND flags -g )
list( APPEND flags -D__divine__ )
list( APPEND flags -D_POSIX_C_SOURCE=2008098L )
list( APPEND flags -D_LITTLE_ENDIAN=1234 )
list( APPEND flags -D_BYTE_ORDER=1234 )

list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR}/ ) # dios
list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR}/libc/include )
list( APPEND flags -isystem${CMAKE_CURRENT_BINARY_DIR}/libc/include )
list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR}/libc/internals )
list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR}/libm/include )
list( APPEND flags -isystem${CMAKE_CURRENT_SOURCE_DIR}/libm/src/ld80 )

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
stringify( "runtime" ${CMAKE_CURRENT_BINARY_DIR} libc/include/sys/divm.h )
stringlist( "runtime" runtime )

include_directories( ${divine_SOURCE_DIR} )
add_definitions( -Wno-overlength-strings ${DIVINE_DEFINES} )
add_library( divine-rt ${divine_SOURCE_DIR}/divine/rt/runtime.cpp
                       ${runtime_FILES} runtime_list.cpp )
set_target_properties( divine-rt PROPERTIES POSITION_INDEPENDENT_CODE ON )
install( TARGETS divine-rt DESTINATION lib )
