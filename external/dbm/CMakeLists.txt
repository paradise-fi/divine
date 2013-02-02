project( dbm )

# disable c++11
string( REPLACE "-std=c++11" "-std=c++98" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} )
string( REPLACE "-std=gnu++11" "-std=gnu++98" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )

add_library( dbm dbm/dbm.c dbm/print.cpp dbm/fed.cpp dbm/fed_dbm.cpp dbm/DBMAllocator.cpp dbm/mingraph_write.c debug/utils.cpp base/intutils.c base/doubles.c base/bitstring.c hash/compute.c hash/tables.cpp )
