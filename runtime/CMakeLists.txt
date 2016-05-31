add_subdirectory( native )

include( stringify )

file( GLOB SRC_RUNTIME LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
      *.cpp divine/*.cpp
      *.h divine/*.h atomic sys/*.h bits/*.h
      pdclib/*.h pdclib/sys/*.h pdclib/bits/*.h pdclib/functions/*/*.c
      pdclib/opt/*/*.c pdclib/posix/*.c pdclib/*.c
      libm/*.c libm/*.h libm/bits/*.h
      libcxxabi/src/* libcxxabi/include/*
      libcxx/src/*.cpp libcxx/src/*.h libcxx/include/* libcxx/ext/*
      filesystem/*.cpp filesystem/*.h )
foreach( f ${SRC_RUNTIME} )
    stringify( "runtime" ${f} )
endforeach()
stringlist( "runtime" runtime )

add_library( libdivinert_str STATIC ${STRINGIFIED} runtime_list.cpp )
if( NOT WIN32 )
    set_target_properties( libdivinert_str PROPERTIES POSITION_INDEPENDENT_CODE ON )
endif()
