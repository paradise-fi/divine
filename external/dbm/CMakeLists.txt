project( dbm )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )

add_library( dbm dbm/dbm.c dbm/print.cpp debug/utils.cpp base/intutils.c )
