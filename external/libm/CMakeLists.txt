include( ../../cmake/stringify.cmake )

file( GLOB src RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.c *.h bits/*.h )
foreach( f ${src} )
    stringify( ${f} )
endforeach()
stringlist( libm )

add_library( m_str STATIC ${STRINGIFIED} libm_list.cpp )
