include( ../../cmake/stringify.cmake )

file( GLOB src RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    src/*.cpp
    src/*.h
    std/*
    ext/*
    )
foreach( f ${src} )
    stringify( "cxx" ${f} )
endforeach()
stringlist( "cxx" libcxx )

add_library( cxx_str STATIC ${STRINGIFIED} libcxx_list.cpp )
