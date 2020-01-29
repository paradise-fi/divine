cmake_minimum_required( VERSION 3.2 )

# file( COPY ) happens at configure time and confuses cmake into thinking
# the old policy is required
cmake_policy( SET CMP0058 NEW )
cmake_policy( SET CMP0057 NEW )

configure_file( ${CMAKE_BINARY_DIR}/config.vars ${CMAKE_BINARY_DIR}/config.vars.stamp COPYONLY )
file( READ ${CMAKE_BINARY_DIR}/config.vars VARS )
foreach( V ${VARS} )
    string( REGEX REPLACE "[ \\t\\n]*([A-Z_]+)=(.*)" "\\1" VAR "${V}" )
    string( REGEX REPLACE "[ \\t\\n]*([A-Z_]+)=(.*)" "\\2" VAL "${V}" )
    string( STRIP ${VAL} VAL )
    if ( ${VAL} STREQUAL "ON" OR ${VAL} STREQUAL "OFF" )
        set( ${VAR} ${VAL} CACHE BOOL "" )
    else()
        set( ${VAR} "${VAL}" CACHE STRING "" )
    endif()
endforeach()

string( REGEX REPLACE "-O[a-z0-9]?" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "" FORCE )

# this must be set BEFORE project
set( CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -DNVALGRIND" CACHE STRING "" )
set( CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -DNVALGRIND" CACHE STRING "" )
set( CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG -DNVALGRIND" CACHE STRING "" )
set( CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG -DNVALGRIND" CACHE STRING "" )
set( CMAKE_CXX_FLAGS_SEMIDBG "-O2 -g" CACHE STRING "" )
set( CMAKE_C_FLAGS_SEMIDBG "-O2 -g" CACHE STRING "" )

set( CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib )
set( CMAKE_SKIP_RPATH OFF )

set( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    "${CMAKE_CURRENT_SOURCE_DIR}/releng/cmake;${CMAKE_CURRENT_SOURCE_DIR}/llvm/cmake;${CMAKE_CURRENT_SOURCE_DIR}/llvm/cmake/modules" )

option( TOOLCHAIN OFF )

set( LLVM_INCLUDE_TESTS OFF CACHE BOOL "" )
set( LLVM_INCLUDE_EXAMPLES OFF CACHE BOOL "" )
set( LLVM_INCLUDE_DOCS OFF CACHE BOOL "" )
set( LLVM_INCLUDE_RUNTIMES OFF CACHE BOOL "" )

set( LLVM_ENABLE_PIC OFF CACHE BOOL "" )
set( LLVM_ENABLE_RTTI ON CACHE BOOL "" )
set( LLVM_ENABLE_TIMESTAMPS OFF CACHE BOOL "" )
set( LLVM_ENABLE_ZLIB ON CACHE BOOL "" )
set( LLVM_ENABLE_TIMESTAMPS OFF CACHE BOOL "" )
set( LLVM_ENABLE_WARNINGS ON CACHE BOOL "" )
set( LLVM_ENABLE_OPTIMIZE OFF CACHE BOOL "" )
set( LLVM_ENABLE_PEDANTIC ON CACHE BOOL "" )
set( LLVM_DISABLE_BINDINGS ON CACHE BOOL "" )

set( CLANG_INCLUDE_DOCS OFF CACHE BOOL "" )

set( LIBCXXABI_USE_LLVM_UNWINDER ON CACHE BOOL "" )
set( LIBCXXABI_USE_COMPILER_RT OFF CACHE BOOL "" )
set( LIBCXX_USE_COMPILER_RT    OFF CACHE BOOL "" )

set( LIBCXXABI_LIBUNWIND_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/dios/libunwind/include" )
set( LIBCXXABI_LIBCXX_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/dios/libcxx/include" )
set( LIBCXXABI_LIBCXX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/dios/libcxx" )

set( LLVM_MAIN_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/llvm" )
set( LLVM_MAIN_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/llvm/include" )
set( LLVM_TARGET_ARCH "host" )

if( WIN32 OR CYGWIN )
  # DLL platform -- put DLLs into bin.
  set( LLVM_SHLIB_OUTPUT_INTDIR ${LLVM_RUNTIME_OUTPUT_INTDIR} )
else()
  set( LLVM_SHLIB_OUTPUT_INTDIR ${LLVM_LIBRARY_OUTPUT_INTDIR} )
endif()

set( LIBCXX_CXX_ABI_LIBNAME "libcxxabi" CACHE STRING "" )
set( LIBCXX_CXX_ABI_INCLUDE_PATHS "${CMAKE_SOURCE_DIR}/dios/libcxxabi/include" CACHE STRING "" )
set( LIBCXX_CXX_ABI_INTREE 1 CACHE BOOL "" )
set( LIBCXX_CXX_ABI "libcxxabi" CACHE STRING "" )

set( STATIC_BUILD OFF CACHE BOOL "link binaries statically" )

project( divine-meta )
enable_language( CXX )
if ( TOOLCHAIN )
    set( CMAKE_CXX_STANDARD 11 )
    set( CMAKE_CXX_STANDARD_REQUIRED ON )
else()
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z" )
endif()
if( LIBCXX_HAS_MUSL_LIBC )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_LIBCPP_HAS_MUSL_LIBC")
endif()

if ( STATIC_BUILD )
    set( CMAKE_LINK_SEARCH_START_STATIC ON )
    # get rid of -rdynamic
    set( CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "" )
    set( CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "" )
    string( APPEND CMAKE_EXE_LINKER_FLAGS " -static" )
endif()

function( build_clang )
  # They are used as destination of target generators.
  set( LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/clang/bin )
  set( LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/clang/lib${LLVM_LIBDIR_SUFFIX} )
  add_subdirectory( clang )
endfunction()

function( build_libcxx dir )
  set( PACKAGE_VERSION "8.0.0" )
  set( LIBCXX_INCLUDE_BENCHMARKS OFF CACHE BOOL "" )
  set( LIBCXX_ABI_VERSION 2 CACHE STRING "" )
  set( LIBCXX_CXX_ABI "libcxxabi" CACHE STRING "" )
  set( LIBCXX_CXX_ABI_INTREE ON CACHE BOOL "" )
  include( FindPythonInterp )
  add_subdirectory( ${dir}/libcxxabi )
  add_subdirectory( ${dir}/libcxx )
endfunction()

function( build_llvm )
  include( AddLLVMDefinitions )
  include( config-ix )
  include( AddLLVM )
  include( TableGen )
  include( HandleLLVMOptions )

  if ( TOOLCHAIN )
    add_subdirectory( dios/libunwind )
    build_libcxx( dios )
  endif()

  # set( PACKAGE_VERSION "${LLVM_PACKAGE_VERSION}" )
  set( PACKAGE_VERSION "7.0.0" )
  add_subdirectory( llvm )
  include_directories( llvm/include ${CMAKE_CURRENT_BINARY_DIR}/llvm/include )
  set( LLVM_TARGETS_TO_BUILD ${LLVM_TARGETS_TO_BUILD} PARENT_SCOPE )
  build_clang()
  include_directories( clang/include ${CMAKE_CURRENT_BINARY_DIR}/clang/include )

  add_subdirectory( lld )
  include_directories( lld/include ${CMAKE_CURRENT_BINARY_DIR}/lld/include )
endfunction()

build_llvm()

project( divine )

set( DIVINE_DEFINES "-DDIVINE_RELAX_WARNINGS=_Pragma( \"GCC diagnostic push\" ) \
 _Pragma( \"GCC diagnostic ignored \\\"-Wold-style-cast\\\"\" ) \
 _Pragma( \"GCC diagnostic ignored \\\"-Wunused-parameter\\\"\" )"
 "-DDIVINE_UNRELAX_WARNINGS=_Pragma( \"GCC diagnostic pop\" )" -D__divm__
 -Wno-c99-extensions -Wno-vla-extension -Wno-gnu-conditional-omitted-operand
 -Wno-c++1z-extensions -Wno-gnu-anonymous-struct -Wno-zero-length-array )

# find_package( LLVM 3.7 REQUIRED )
find_package( Perl )
find_package( ODBC )
find_package( PkgConfig )
find_package( Z3 )

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

opt( OPT_TBBMALLOC "use Intel TBB memory allocator" OFF )
opt( OPT_SIM "build the interactive debugger/simulator" ON )
opt( OPT_SQL "enable ODBC-based database support" ${ODBC_FOUND} )
opt( OPT_Z3 "enable the Z3 solver backend (needs a Z3 installation)" ${Z3_FOUND} )
opt( OPT_STP "enable the STP solver backend" ON )

if ( OPT_SIM AND NOT TOOLCHAIN )
  if ( STATIC_BUILD )
    find_library( LIBEDIT libedit.a )
    find_library( LIBTINFO NAMES libtinfo.a libncurses.a libncursesw.a )
  else()
    find_library( LIBEDIT edit )
    find_library( LIBTINFO NAMES tinfo ncurses ncursesw )
  endif()

  find_package( Curses REQUIRED )
  include_directories( ${CURSES_INCLUDE_PATH} )

  if ( NOT LIBEDIT OR NOT LIBTINFO )
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

if ( UNIX AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
     CMAKE_GENERATOR STREQUAL "Ninja")
  add_compile_options( -fcolor-diagnostics )
endif()

include( bricks/support.cmake )
bricks_check_dirent()
set( BRICK_USED_LLVM_LIBS ${BRICK_LLVM_LIBS} )
add_definitions( -DBRICKS_HAVE_LLVM )
include_directories( bricks )

pkg_check_modules( VALGRIND valgrind )

if ( VALGRIND_FOUND )
  include_directories( ${VALGRIND_INCLUDE_DIRS} )
else()
  message( WARNING "Valgrind (headers) not found! Pool debugging will not work." )
  add_definitions( -DNVALGRIND )
endif()

if ( OPT_STP )
  set( STP_INCLUDES ${divine_SOURCE_DIR}/stp/include ${divine_BINARY_DIR}/stp/include
                    ${divine_SOURCE_DIR}/stp/lib ${divine_SOURCE_DIR} ${divine_BINARY_DIR}/cryptoms )
  set( STP_LIBRARIES libstp )
else()
  set( STP_INCLUDES "" )
  set( STP_LIBRARIES "" )
endif()

set( DIVINE_INCLUDES ${divine_SOURCE_DIR} ${divine_BINARY_DIR} )
set( DIVINE_SYS_INCLUDES ${Z3_INCLUDE_DIRS} ${STP_INCLUDES} )

set( TEST_WRAPPER "-c" )

add_custom_target( check )
add_custom_target( unit )

add_custom_target( llvm-utils )
add_dependencies( llvm-utils llvm-dis llvm-as llvm-nm llvm-diff llvm-objdump llc )

if ( NOT TOOLCHAIN )

  if( OPT_TBBMALLOC )
    add_subdirectory( external/tbbmalloc )
  endif()

  if( OPT_SQL )
    add_definitions( "-DNANODBC_USE_CPP11" )
    link_directories( ${ODBC_LIBRARY_DIRS} )
    add_subdirectory( external/nanodbc )
  endif()

  add_compile_options( -Wall -Wold-style-cast -Wno-unused-function
                       -Wno-gnu-zero-variadic-macro-arguments )

  if( DEV_WARNINGS )
    add_compile_options( -Wextra )
  endif()

  if( DEV_WERROR )
    add_compile_options( -Werror )
  endif()

  include( releng/version.cmake )

  add_subdirectory( doc )
  add_subdirectory( dios )
  if ( OPT_STP )
    add_subdirectory( minisat )
    add_subdirectory( cryptoms )
    add_subdirectory( stp )
  endif()
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
  message( "## Build options:${BUILDOPTS}" )
  message( "## -----------------------------------------------------------------" )

  test_bricks( bricks ${DIVINE_DEFINES} )
  benchmark_bricks( bricks )

  add_custom_target( unit_bricks
    COMMAND sh ${TEST_WRAPPER} ${WITH_LCOV} ${CMAKE_CURRENT_BINARY_DIR}/test-bricks
    VERBATIM
    USES_TERMINAL
    DEPENDS test-bricks )

  add_dependencies( unit unit_bricks )

endif()

# track extra dependencies for cmake

if ( EXISTS ${CMAKE_SOURCE_DIR}/_darcs )
  configure_file( _darcs/hashed_inventory ${CMAKE_BINARY_DIR}/_darcs_inv COPYONLY )
  configure_file( _darcs/patches/pending  ${CMAKE_BINARY_DIR}/_darcs_pending COPYONLY )
endif()
