project( dbm )

string( REPLACE "-Wold-style-cast" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include ${Boost_INCLUDE_DIR} )

add_library( dbm dbm/dbm.c dbm/print.cpp dbm/fed.cpp dbm/fed_dbm.cpp dbm/DBMAllocator.cpp dbm/mingraph_write.c debug/utils.cpp base/intutils.c base/doubles.c base/bitstring.c hash/compute.c hash/tables.cpp )
