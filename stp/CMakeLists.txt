# AUTHORS: Dan Liew, Ryan Gvostes, Mate Soos
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

add_definitions( -D__STDC_LIMIT_MACROS -DUSE_CRYPTOMINISAT )
add_compile_options( -Wno-empty-body -Wno-old-style-cast -Wno-infinite-recursion
                     -Wno-dangling-else -Wno-unused-variable -Wno-unused-parameter
                     -Wno-tautological-constant-out-of-range-compare -Wno-sign-compare
                     -Wno-pointer-sign -Wno-implicit-function-declaration
                     -Wno-macro-redefined )

include_directories( ${STP_INCLUDES} )

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/stp/config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/include/stp/config.h"
)

add_subdirectory(lib)
