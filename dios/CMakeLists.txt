#add_subdirectory( native )

include( stringify )

file( GLOB H_RUNTIME LIST_DIRECTORIES false
      sys/*.hpp vfs/*.h vfs/*.hpp macro/*
      include/*.h include/_PDCLIB/*.h include/util/*.hpp include/rst/*.h include/rst/*.def
      include/sys/*.h include/sys/*.def include/bits/*.h include/machine/*.h
      include/netinet/*.h
      libcxxabi/include/*
      libcxxabi/src/*.h libcxxabi/src/*.hpp libcxxabi/src/*.ipp
      libcxx/include/* libcxx/ext/* libcxx/include/experimental/*
      libcxx/src/*.h
      libcxx/include/support/xlocale/*.h libcxx/src/support/atomic_support.h
      libunwind/include/* libunwind/include/mach-o/* )

file( GLOB SRC_dios sys/*.cpp vfs/*.cpp )

file( GLOB SRC_librst rst/*.cpp )
file( GLOB SRC_libc libc/*/*.c )
file( GLOB SRC_libc_cpp libc/*/*.cpp )
file( GLOB SRC_libm libm/*.c )
file( GLOB SRC_libcxxabi libcxxabi/src/*.cpp )
file( GLOB SRC_libcxx libcxx/src/*.cpp  )
file( GLOB SRC_libpthread libpthread/*.cpp )
file( GLOB SRC_host host/*.c )

set( SYSINC ${CMAKE_CURRENT_BINARY_DIR}/include/sys/ )
set( LIBCINC ${CMAKE_CURRENT_SOURCE_DIR}/include/ )
set( HOSTABI ${SYSINC}hostabi.h )
set( VMINC ${SYSINC}divm.h ${SYSINC}vmutil.h ${SYSINC}lart.h )

set( CLANG_HDRS clang/lib/Headers/ )
set( STDATOMIC ${CMAKE_CURRENT_BINARY_DIR}/include/stdatomic.h )

list( REMOVE_ITEM H_RUNTIME ${LIBCINC}sys/divm.h ${LIBCINC}sys/vmutil.h ${LIBCINC}sys/lart.h
                            ${LIBCINC}stdatomic.h )

macro( mkobjs var flags )
  foreach( f ${SRC_${var}} )
    string( REGEX REPLACE "\\.[a-z]+" ".bc" out ${f} )
    file( RELATIVE_PATH out "${CMAKE_CURRENT_SOURCE_DIR}" "${out}" )
    list( APPEND BC_${var} bc/${out} )
    add_custom_command(
        DEPENDS runtime-cc ${f} ${H_RUNTIME} ${HOSTABI} ${VMINC} ${STDATOMIC}
        OUTPUT bc/${out}
        COMMAND mkdir -p bc
        COMMAND runtime-cc ${divine_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                           ${f} bc/${out} ${flags} )
    stringify( "dios" "." ${f} )
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

  stringify( "dios" "${CMAKE_CURRENT_BINARY_DIR}/bc" "${lib}.a" )
endmacro()

macro( copyhdr srcdir dstdir hdr )
  add_custom_command(
    OUTPUT ${dstdir}/${hdr}
    DEPENDS ${divine_SOURCE_DIR}/${srcdir}/${hdr}
    VERBATIM
    COMMAND cmake -E copy_if_different ${divine_SOURCE_DIR}/${srcdir}/${hdr} ${dstdir}/${hdr} )
endmacro()

copyhdr( divine/vm ${SYSINC} divm.h )
copyhdr( divine/vm ${SYSINC} vmutil.h )
copyhdr( lart ${SYSINC} lart.h )
copyhdr( ${CLANG_HDRS} ${CMAKE_CURRENT_BINARY_DIR}/include stdatomic.h )

add_custom_command(
  OUTPUT ${HOSTABI}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/hostabi.pl
  VERBATIM
  COMMAND perl ${CMAKE_CURRENT_SOURCE_DIR}/hostabi.pl ${CMAKE_C_COMPILER} > ${HOSTABI}
)

add_library( dios-host STATIC ${SRC_host} )
target_include_directories( dios-host BEFORE PRIVATE host )
stringify( "dios" "${CMAKE_CURRENT_BINARY_DIR}" "libdios-host.a" )

include( compile-flags.cmake )

include_directories( ${divine_SOURCE_DIR} )
add_definitions( -Wno-overlength-strings ${DIVINE_DEFINES} )
file( GLOB SRC_rt ${divine_SOURCE_DIR}/divine/rt/*.cpp )
add_library( divine-rt ${SRC_rt} ${dios_FILES} dios_list.cpp )
target_link_libraries( divine-rt divine-cc )
set_target_properties( divine-rt PROPERTIES POSITION_INDEPENDENT_CODE ON )
install( TARGETS divine-rt DESTINATION lib )
