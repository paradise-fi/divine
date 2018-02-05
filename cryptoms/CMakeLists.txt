# Copyright (c) 2017, Mate Soos
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

include_directories(
    ${PROJECT_SOURCE_DIR}
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_compile_options( -Wno-old-style-cast -Wno-unused-parameter -Wno-mismatched-tags )

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/GitSHA1.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/GitSHA1.cpp" @ONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cryptominisat.h.in" "${CMAKE_CURRENT_BINARY_DIR}/cryptominisat5/cryptominisat.h" @ONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/solvertypesmini.h.in" "${CMAKE_CURRENT_BINARY_DIR}/cryptominisat5/solvertypesmini.h" @ONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cryptominisat_c.h.in" "${CMAKE_CURRENT_BINARY_DIR}/cryptominisat5/cryptominisat_c.h" @ONLY)

set(cryptoms_lib_files
    cnf.cpp
    propengine.cpp
    varreplacer.cpp
    clausecleaner.cpp
    clauseusagestats.cpp
    prober.cpp
    occsimplifier.cpp
    subsumestrengthen.cpp
    clauseallocator.cpp
    sccfinder.cpp
    solverconf.cpp
    distillerallwithall.cpp
    distillerlongwithimpl.cpp
    str_impl_w_impl_stamp.cpp
    solutionextender.cpp
    completedetachreattacher.cpp
    searcher.cpp
    solver.cpp
    sqlstats.cpp
    implcache.cpp
    stamp.cpp
    compfinder.cpp
    comphandler.cpp
    hyperengine.cpp
    subsumeimplicit.cpp
    datasync.cpp
    reducedb.cpp
    clausedumper.cpp
    bva.cpp
    intree.cpp
    features_calc.cpp
    features_to_reconf.cpp
    solvefeatures.cpp
    searchstats.cpp
    xorfinder.cpp
    cryptominisat_c.cpp
#    watcharray.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/GitSHA1.cpp
)

set(cryptoms_lib_link_libs "")

if (USE_GAUSS)
    SET(cryptoms_lib_files ${cryptoms_lib_files}
        gaussian.cpp
        matrixfinder.cpp
    )
endif()

if (M4RI_FOUND)
    include_directories(${M4RI_INCLUDE_DIRS})

    SET(cryptoms_lib_files ${cryptoms_lib_files} toplevelgauss.cpp)
    SET(cryptoms_lib_link_libs ${cryptoms_lib_link_libs} ${M4RI_LIBRARIES})
endif (M4RI_FOUND)

add_library(libcryptominisat5 STATIC
    ${cryptoms_lib_files}
    cryptominisat.cpp
)

# indicate that we depend on pthread, and compile in the actual library
target_link_libraries(libcryptominisat5
    LINK_PUBLIC ${cryptoms_lib_link_libs}
    LINK_PUBLIC ${CMAKE_THREAD_LIBS_INIT}
)

IF (ZLIB_FOUND)
    SET(cryptoms_exec_link_libs ${cryptoms_exec_link_libs} ${ZLIB_LIBRARY})
ENDIF()
