include( ../../cmake/stringify.cmake )

file( GLOB src RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cc *.h )
foreach( f ${src} )
    stringify( ${f} )
endforeach()
stringlist( libsupcpp )

add_library( supc++_str STATIC ${STRINGIFIED} libsupcpp_list.cpp )
