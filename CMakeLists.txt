cmake_minimum_required( VERSION 3.2 )

# file( COPY ) happens at configure time and confuses cmake into thinking
# the old policy is required
cmake_policy( SET CMP0058 NEW )

# this must be set BEFORE project
set( CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -DNVALGRIND" CACHE STRING "" )
set( CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -DNVALGRIND" CACHE STRING "" )
set( CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG -DNVALGRIND" CACHE STRING "" )
set( CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG -DNVALGRIND" CACHE STRING "" )
set( CMAKE_CXX_FLAGS_SEMIDBG "-O2 -g" CACHE STRING "" )
set( CMAKE_C_FLAGS_SEMIDBG "-O2 -g" CACHE STRING "" )

set( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    "${CMAKE_CURRENT_SOURCE_DIR}/releng/cmake;${CMAKE_CURRENT_SOURCE_DIR}/llvm/cmake;${CMAKE_CURRENT_SOURCE_DIR}/llvm/cmake/modules" )

option( TOOLCHAIN OFF )

set( LLVM_INCLUDE_TESTS OFF CACHE BOOL "" )
set( LLVM_INCLUDE_EXAMPLES OFF CACHE BOOL "" )
set( LLVM_INCLUDE_DOCS OFF CACHE BOOL "" )

set( LLVM_ENABLE_PIC OFF CACHE BOOL "" )
set( LLVM_ENABLE_RTTI ON CACHE BOOL "" )
set( LLVM_ENABLE_TIMESTAMPS OFF CACHE BOOL "" )
set( LLVM_ENABLE_ZLIB OFF CACHE BOOL "" )
set( LLVM_ENABLE_TIMESTAMPS OFF CACHE BOOL "" )
set( LLVM_ENABLE_WARNINGS ON CACHE BOOL "" )
set( LLVM_ENABLE_PEDANTIC ON CACHE BOOL "" )

set( CLANG_INCLUDE_DOCS OFF CACHE BOOL "" )
set( LIBCXXABI_USE_LLVM_UNWINDER ON CACHE BOOL "" )

set( LIBCXXABI_LIBUNWIND_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/runtime/libunwind/include" )
set( LIBCXXABI_LIBCXX_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/runtime/libcxx/include" )
set( LIBCXXABI_LIBCXX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/runtime/libcxx" )

set( LLVM_MAIN_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/llvm" )
set( LLVM_MAIN_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/llvm/include" )
set( LLVM_TARGET_ARCH "host" )
# They are used as destination of target generators.
set( LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/clang/bin )
set( LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/clang/lib${LLVM_LIBDIR_SUFFIX} )
if( WIN32 OR CYGWIN )
  # DLL platform -- put DLLs into bin.
  set( LLVM_SHLIB_OUTPUT_INTDIR ${LLVM_RUNTIME_OUTPUT_INTDIR} )
else()
  set( LLVM_SHLIB_OUTPUT_INTDIR ${LLVM_LIBRARY_OUTPUT_INTDIR} )
endif()

set( LIBCXX_CXX_ABI_LIBNAME "libcxxabi" CACHE STRING "" )
set( LIBCXX_CXX_ABI_INCLUDE_PATHS "${CMAKE_SOURCE_DIR}/runtime/libcxxabi/include" CACHE STRING "" )
set( LIBCXX_CXX_ABI_INTREE 1 CACHE BOOL "" )
set( LIBCXX_CXX_ABI "libcxxabi" CACHE STRING "" )

project( divine-meta )
enable_language( CXX )
set( CMAKE_CXX_STANDARD 14 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

function(build_llvm)
  include( AddLLVMDefinitions )
  include( config-ix )
  include( AddLLVM )
  include( TableGen )
  include( HandleLLVMOptions )

  # set( PACKAGE_VERSION "${LLVM_PACKAGE_VERSION}" )
  set( PACKAGE_VERSION "3.7.0" )
  add_subdirectory( llvm )
  include_directories( llvm/include ${CMAKE_CURRENT_BINARY_DIR}/llvm/include )
  add_subdirectory( clang )
  include_directories( clang/include ${CMAKE_CURRENT_BINARY_DIR}/clang/include )

  if ( TOOLCHAIN )
    # defines for the sake of compiler-rt, normaly from llvm-config
    set( LLVM_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/llvm )
    set( LLVM_TOOLS_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/llvm/bin )
    set( LLVM_LIBRARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/llvm/lib )
    set( LLVM_MAIN_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/llvm )
    set( TARGET_TRIPLE ${LLVM_DEFAULT_TARGET_TRIPLE} )
    add_subdirectory( compiler-rt )
    include_directories( compiler-rt/include ${CMAKE_CURRENT_BINARY_DIR}/compiler-rt/include )

    add_subdirectory( runtime/libunwind )
    add_subdirectory( runtime/libcxxabi )
    add_subdirectory( runtime/libcxx )
  endif()
endfunction()

build_llvm()

set( PACKAGE_VERSION 4.0 )

mark_as_advanced( FORCE
  CMAKE_CXX_FLAGS_RELWITHDEBUG CMAKE_C_FLAGS_RELWITHDEBUG
  CMAKE_CXX_FLAGS_RELWITHASSERT CMAKE_C_FLAGS_RELWITHASSERT )

project( divine )

set( DIVINE_DEFINES "-DDIVINE_RELAX_WARNINGS=_Pragma( \"GCC diagnostic push\" ) \
 _Pragma( \"GCC diagnostic ignored \\\"-Wold-style-cast\\\"\" ) \
 _Pragma( \"GCC diagnostic ignored \\\"-Wunused-parameter\\\"\" )"
 "-DDIVINE_UNRELAX_WARNINGS=_Pragma( \"GCC diagnostic pop\" )"
 -Wno-c99-extensions -Wno-vla-extension -Wno-gnu-conditional-omitted-operand
 -Wno-c++1z-extensions -Wno-gnu-anonymous-struct -Wno-zero-length-array )

# find_package( LLVM 3.7 REQUIRED )
find_package( Perl )
find_package( ODBC )
find_package( PkgConfig )

if ( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE "Release"
    CACHE STRING
    "One of: Debug Release RelWithDebInfo SemiDbg." FORCE )
endif()

macro( opt name help defval )
  option( ${opt} "${name}" "${help}" "${defval}" )
  list( APPEND OPTIONS ${name} )
endmacro()

set( IS_DEBUG OFF )
set( IS_NDEBUG ON )

if( CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "SemiDbg" )
  set( IS_DEBUG ON )
  set( IS_NDEBUG OFF )
endif()

opt( OPT_TBBMALLOC "use Intel TBB memory allocator" ${IS_NDEBUG} )
opt( OPT_SIM "build the interactive debugger/simulator" ON )

opt( OPT_ASSERT "enable assertions" ${IS_DEBUG} )
opt( ALG_MAP "" OFF )
opt( ALG_OWCTY "" ON )
opt( ALG_REACHABILITY "" ON )
opt( ALG_WEAKREACHABILITY "" OFF )
opt( ALG_EXPLICIT "" OFF )
opt( ALG_CSDR "" ON )
opt( ALG_NDFS "" OFF )
opt( ALG_METRICS "" ON )
opt( OPT_SQL "enable ODBC-based database reports" OFF ) # ${ODBC_FOUND}

if ( OPT_SIM AND NOT TOOLCHAIN )
  find_package( Curses REQUIRED )
  find_library( LIBEDIT edit )
  include_directories( ${CURSES_INCLUDE_PATH} )

  if ( NOT LIBEDIT )
    message( SEND_ERROR "libedit is required for 'divine sim' (-DOPT_SIM=ON)" )
  endif()
endif()

option( DEV_WARNINGS "enable extra compiler warnings" ${IS_DEBUG} )
option( DEV_WERROR "enable -Werror" ${IS_DEBUG} )
option( BUILD_SHARED_LIBS "build shared libraries" OFF )

foreach( opt ${OPTIONS} )
  if ( ${${opt}} )
    add_definitions( "-D${opt}=1" )
    set( BUILDOPTS "${BUILDOPTS} ${opt}" )
  else()
    add_definitions( "-D${opt}=0" )
  endif()
endforeach()

add_compile_options( -Wall -Wold-style-cast -Wno-unused-function -Wno-gnu-zero-variadic-macro-arguments )
if( DEV_WARNINGS )
  add_compile_options( -Wextra )
endif()

if( DEV_WERROR )
  add_compile_options( -Werror )
endif()

if ( UNIX AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
     CMAKE_GENERATOR STREQUAL "Ninja")
  add_compile_options( -fcolor-diagnostics )
endif()

include( bricks/support.cmake )
bricks_check_dirent()
include_directories( bricks )

pkg_check_modules( VALGRIND valgrind )

if ( VALGRIND_FOUND )
  include_directories( ${VALGRIND_INCLUDE_DIRS} )
else()
  message( WARNING "Valgrind (headers) not found! Pool debugging will not work." )
  add_definitions( -DNVALGRIND )
endif()

add_definitions( -DBRICKS_HAVE_LLVM )

# link_directories( ${LLVM_LIBRARY_DIRS} )
# this is not very nice but it works and ncurses & zlib break builds under NixOS
# list( REMOVE_ITEM LLVM_SYSLIBS "z" "ncurses" )
# suppress warnings coming from LLVM headers
# add_compile_options( "-isystem ${LLVM_INCLUDE_DIRS}" )

set( DIVINE_INCLUDES ${divine_SOURCE_DIR} ${divine_BINARY_DIR} )

# set( TEST_WRAPPER "${CMAKE_SOURCE_DIR}/nix/test-wrapper.sh" )
set( TEST_WRAPPER "-c" )

add_custom_target( check )
add_custom_target( unit )

add_custom_target( llvm-utils )
add_dependencies( llvm-utils llvm-dis llvm-as llvm-nm llvm-diff llvm-objdump llc )

if( OPT_TBBMALLOC )
  add_subdirectory( external/tbbmalloc )
endif()

if( OPT_SQL )
  add_definitions( "-DNANODBC_USE_CPP11" )
  link_directories( ${ODBC_LIBRARY_DIRS} )
  add_subdirectory( external/nanodbc )
endif()

include( releng/version.cmake )

add_subdirectory( doc )
add_subdirectory( runtime )
add_subdirectory( lart )
add_subdirectory( divine )
add_subdirectory( tools )
add_subdirectory( test )

foreach( p ui vm cc )
    add_dependencies( divine-${p} intrinsics_gen )
endforeach()
add_dependencies( liblart intrinsics_gen )

add_dependencies( check unit )
add_dependencies( check functional )

include( releng/install.cmake )

message( "## -----------------------------------------------------------------" )
message( "## Version: ${DIVINE_VERSION}" )
message( "## Build type: ${CMAKE_BUILD_TYPE}" )
message( "## Build options: ${BUILDOPTS}" )
message( "## -----------------------------------------------------------------" )

test_bricks( bricks ${DIVINE_DEFINES} )
benchmark_bricks( bricks )

add_custom_target( unit_bricks
    COMMAND sh ${TEST_WRAPPER} ${WITH_LCOV} ${CMAKE_CURRENT_BINARY_DIR}/test-bricks
    VERBATIM
    USES_TERMINAL
    DEPENDS test-bricks )

add_dependencies( unit unit_bricks )
