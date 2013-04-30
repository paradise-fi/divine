include( ../../cmake/stringify.cmake )

file( GLOB src RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h functions/*/*.c opt/*/*.c posix/*.c *.c )
foreach( f ${src} )
    stringify( ${f} )
endforeach()
stringlist( pdclib )

add_library( pdclib_str STATIC ${STRINGIFIED} pdclib_list.cpp )
