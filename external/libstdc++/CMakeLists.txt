include( ../../cmake/stringify.cmake )

file( GLOB src RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*/*.cc src/*/*.h
    backward/* decimal/*
    c_global/*
    bits/* parallel/* debug/*
    std/* tr1/* tr2/* ext/* )
foreach( f ${src} )
    stringify( ${f} )
endforeach()
stringlist( libstdcpp )

add_library( stdc++_str STATIC ${STRINGIFIED} libstdcpp_list.cpp )
