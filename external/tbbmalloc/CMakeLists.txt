include_directories( . )
add_definitions( -DDO_ITT_NOTIFY=0 -DUSE_PTHREAD )
add_library( tbbmalloc STATIC backend.cpp frontend.cpp backref.cpp large_objects.cpp proxy.cpp tbb_function_replacement.cpp tbbmalloc.cpp )
