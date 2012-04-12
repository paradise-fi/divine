project( buddy )

add_definitions( ${OPT_FLAGS} ${CXX_FLAGS} -DHAVE_CONFIG_H )

include_directories( src . )

add_library( buddy src/bddio.c src/bddop.c src/bvec.c src/cache.c src/cppext.cxx src/fdd.c src/imatrix.c src/kernel.c src/pairs.c src/prime.c src/reorder.c src/tree.c )
