include( stringify )

file( GLOB src RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.c *.h bits/*.h )
foreach( f ${src} )
    stringify( "m" ${f} )
endforeach()
stringlist( "m" libm )

add_library( m_str STATIC ${STRINGIFIED} libm_list.cpp )
