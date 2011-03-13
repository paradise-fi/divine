project( ltl2ba C )

add_definitions( ${OPT_FLAGS} ${CXX_FLAGS} -ansi -DNXT )

add_library( ltl2ba parse.c lex.c main.c trans.c buchi.c set.c mem.c rewrt.c cache.c alternating.c generalized.c )
