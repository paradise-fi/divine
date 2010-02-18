find_package( BISON )
find_package( FLEX )

set( LEX ${CMAKE_CURRENT_BINARY_DIR}/mulexer.cpp )
set( PARSE ${CMAKE_CURRENT_BINARY_DIR}/muparser.cpp )

find_program( BYACC "byacc" )
set( BISON_EXECUTABLE ${BYACC} ) # ZOMG HACK!

file( GLOB code *.C )
bison_target( muparser mu.y ${PARSE} )
flex_target( mulexer mu.l ${LEX} )

include_directories( ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} )

add_library( murphi STATIC ${code} ${PARSE} ${LEX} )
