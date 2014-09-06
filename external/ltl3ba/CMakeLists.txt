project( ltl3ba )

add_definitions( ${OPT_FLAGS} ${CXX_FLAGS} -DNXT -Wno-format-security -D__ltl3ba_internal )

add_subdirectory( buddy )

include_directories( buddy/src )

add_library( ltl3ba parse.cpp lex.cpp main.cpp trans.cpp buchi.cpp cset.cpp set.cpp mem.cpp rewrt.cpp cache.cpp alternating.cpp generalized.cpp optim.cpp queue.cpp )

target_link_libraries( ltl3ba buddy )
