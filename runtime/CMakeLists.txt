#add_subdirectory( native )

include( stringify )

file( GLOB H_RUNTIME LIST_DIRECTORIES false
      *.h dios/*.hpp dios/core/*.hpp dios/core/*.def dios/lib/*.hpp
      dios/macro/*
      atomic sys/*.h bits/*.h
      abstract/*.hpp abstract/*.h
      libc/include/*.h libc/internals/*.h
      libc/include/sys/*.h libc/include/sys/*.def libc/include/bits/*.h
      libm/include/*.h libm/include/machine/*.h
      libcxxabi/include/*
      libcxxabi/src/*.h libcxxabi/src/*.hpp libcxxabi/src/*.ipp
      libcxx/include/* libcxx/ext/* libcxx/include/experimental/*
      libcxx/src/*.h
      libcxx/include/support/xlocale/*.h libcxx/src/support/atomic_support.h
      libunwind/include/* libunwind/include/mach-o/* dios/filesystem/*.h )

file( GLOB SRC_libdios
      dios/*.cpp dios/core/*.cpp dios/filesystem/*.cpp )

file( GLOB SRC_libabstract abstract/*.cpp abstract/*.c )
file( GLOB SRC_libc libc/functions/*/*.c )
file( GLOB SRC_libc_cpp libc/functions/*/*.cpp )
file( GLOB SRC_libm libm/src/*.c )
file( GLOB SRC_libcxxabi libcxxabi/src/*.cpp )
file( GLOB SRC_libcxx libcxx/src/*.cpp  )

set( SYSINC ${CMAKE_CURRENT_BINARY_DIR}/libc/include/sys/ )
set( LIBCINC ${CMAKE_CURRENT_SOURCE_DIR}/libc/include/ )
set( HOSTABI ${SYSINC}hostabi.h )
set( VMINC ${SYSINC}divm.h ${SYSINC}lart.h ${SYSINC}vmutil.h )

list( REMOVE_ITEM H_RUNTIME ${LIBCINC}sys/divm.h ${LIBCINC}sys/vmutil.h ${LIBCINC}sys/lart.h )

macro( mkobjs var flags )
  foreach( f ${SRC_${var}} )
    string( REGEX REPLACE "\\.[a-z]+" ".bc" out ${f} )
    file( RELATIVE_PATH out "${CMAKE_CURRENT_SOURCE_DIR}" "${out}" )
    list( APPEND BC_${var} bc/${out} )
    add_custom_command(
        DEPENDS runtime-cc ${f} ${H_RUNTIME} ${HOSTABI} ${VMINC}
        OUTPUT bc/${out}
        COMMAND mkdir -p bc
        COMMAND runtime-cc ${divine_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                           ${f} bc/${out} ${flags} )
    stringify( "runtime" "." ${f} )
  endforeach()
endmacro()

macro( mklib lib )
  foreach( var ${ARGN} )
    list( APPEND BC_${lib} ${BC_${var}} )
  endforeach()

  add_custom_command(
    DEPENDS runtime-ld ${BC_${lib}}
    OUTPUT "bc/${lib}.a"
    COMMAND runtime-ld bc/${lib}.a ${BC_${lib}}
  )

  stringify( "runtime" "${CMAKE_CURRENT_BINARY_DIR}/bc" "${lib}.a" )
endmacro()

foreach( hdr divm.h lart.h vmutil.h )
  add_custom_command(
    OUTPUT ${SYSINC}/${hdr}
    DEPENDS ${divine_SOURCE_DIR}/divine/vm/${hdr}
    VERBATIM
    COMMAND cmake -E copy_if_different ${divine_SOURCE_DIR}/divine/vm/${hdr} ${SYSINC}/${hdr} )
endforeach()

add_custom_command(
  OUTPUT ${HOSTABI}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/hostabi.pl
  VERBATIM
  COMMAND perl ${CMAKE_CURRENT_SOURCE_DIR}/hostabi.pl ${CMAKE_C_COMPILER} > ${HOSTABI}
)

include( compile-rt.flags )

include_directories( ${divine_SOURCE_DIR} )
add_definitions( -Wno-overlength-strings ${DIVINE_DEFINES} )
add_library( divine-rt ${divine_SOURCE_DIR}/divine/rt/runtime.cpp
                       ${runtime_FILES} runtime_list.cpp )
set_target_properties( divine-rt PROPERTIES POSITION_INDEPENDENT_CODE ON )
install( TARGETS divine-rt DESTINATION lib )
