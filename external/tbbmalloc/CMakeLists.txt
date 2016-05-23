string(REPLACE "-Wold-style-cast" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
include_directories( . )
add_definitions( -DDO_ITT_NOTIFY=0 )
if ( WIN32 )
  add_definitions( -DUSE_WINTHREAD )
else()
  add_definitions( -DUSE_PTHREAD )
endif()

add_library( tbbmalloc STATIC backend.cpp frontend.cpp backref.cpp large_objects.cpp proxy.cpp tbb_function_replacement.cpp tbbmalloc.cpp )

if ( "${CMAKE_SYSTEM_NAME}" STREQUAL "Linux" )
    target_link_libraries( tbbmalloc dl )
endif()
