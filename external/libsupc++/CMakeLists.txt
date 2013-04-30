include( ../../cmake/stringify.cmake )

file( GLOB src RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cc *.cpp *.h *.cxx typeinfo exception new )
foreach( f ${src} )
    stringify( ${f} )
endforeach()
stringlist( libsupcpp )

add_library( supc++_str STATIC ${STRINGIFIED} libsupcpp_list.cpp )
