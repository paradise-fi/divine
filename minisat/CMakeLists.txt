#--------------------------------------------------------------------------------------------------
# Dependencies:

include_directories(${divine_SOURCE_DIR})

#--------------------------------------------------------------------------------------------------
# Compile flags:

add_definitions(-D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS )
add_compile_options( -Wno-old-style-cast -Wno-reserved-user-defined-literal )

#--------------------------------------------------------------------------------------------------
# Build Targets:

set(MINISAT_LIB_SOURCES
    utils/Options.cc
    utils/System.cc
    core/Solver.cc
    simp/SimpSolver.cc)

add_library(libminisat STATIC ${MINISAT_LIB_SOURCES})

set_target_properties(libminisat PROPERTIES OUTPUT_NAME "minisat"
                                            POSITION_INDEPENDENT_CODE ON )
