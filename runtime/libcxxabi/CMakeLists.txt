include( stringify )

file( GLOB src RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    src/*
    include/*
    )
foreach( f ${src} )
    stringify( "cxxabi" ${f} )
endforeach()
stringlist( "cxxabi" libcxxabi )

add_library( cxxabi_str STATIC ${STRINGIFIED} libcxxabi_list.cpp )
