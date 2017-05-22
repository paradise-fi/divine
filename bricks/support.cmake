## This file contains support functions and macros for making use of bricks
## easier with cmake-based projects. Call include(bricks/support.cmake) in your
## toplevel CMakeLists.txt to access those functions.

set( BRICK_USED_LLVM_LIBS )
set( BRICK_LLVM_LIBS LLVMCore LLVMSupport LLVMIRReader LLVMBitReader
                     LLVMBitWriter LLVMLinker LLVMObject )

function( update_file name content )
  if( EXISTS ${name} )
    file( READ ${name} old )
  endif()

  if( NOT "${old}" STREQUAL "${content}" )
    file( WRITE ${name} ${content} )
  endif()
endfunction()

function( bricks_make_runner name main flags )
  set( file "${CMAKE_CURRENT_BINARY_DIR}/${name}-runner.cpp" )
  set( n 0 )
  set( libs "" )

  foreach( src ${ARGN} )
    math( EXPR n "${n} + 1" )
    set( fn "${CMAKE_CURRENT_BINARY_DIR}/${name}-runner-${n}.cpp" )
    update_file( "${fn}" "#include <${src}>" )
    set_source_files_properties( ${fn} PROPERTIES COMPILE_FLAGS ${flags} )
    add_library( "${name}-${n}" SHARED EXCLUDE_FROM_ALL ${fn} )
    list( APPEND libs ${name}-${n} )
  endforeach( src )

  set( main "
    namespace brick { namespace unittest {
        std::vector< TestCaseBase * > *testcases\;
        std::set< std::string > *registered\;
    } }
    int main( int argc, const char **argv ) {
      int r = 1\;
      ${main}
      return r\;
    }"
  )

  update_file( ${file} "${main}" )

  add_executable( ${name} EXCLUDE_FROM_ALL ${file} )
  target_link_libraries( ${name} ${libs} )
  set_source_files_properties( ${file} PROPERTIES COMPILE_FLAGS ${flags} )
endfunction()

# Create a unit test driver for a bunch of header files. Syntax:
#
#    bricks_unittest( driver_name header1 header2 ... )
#
# This will get you an executable target driver_name that you can run
# to run the testsuite.

function( bricks_unittest name )
  bricks_make_runner( ${name} "r = brick::unittest::run( argc, argv )\;"
                      "-UNDEBUG -DBRICK_UNITTEST_REG -include brick-unittest" ${ARGN} )
endfunction()

function( bricks_benchmark name )
  bricks_make_runner( ${name} "brick-benchmark" "brick::benchmark::run( argc, argv )\;"
                      "-DBRICK_BENCHMARK_REG -include brick-benchmark" ${ARGN} )
  target_link_libraries( ${name} rt )
endfunction()

# Register a target "test-bricks" that builds and runs all the unit tests
# included with bricks. Use test_bricks( directory_with_bricks ). Also note
# that if you write your own unit tests using brick-unittest, the tests of
# any bricks that you use in the tested units will be automatically included in
# your testsuite.

function( test_bricks dir )
  include_directories( ${dir} )
  add_definitions( ${ARGN} )
  file( GLOB SRC "${dir}/brick-*[a-z0-9]" )
  bricks_unittest( test-bricks ${SRC} )
  target_link_libraries( test-bricks pthread ${BRICK_USED_LLVM_LIBS} )
endfunction()

function( benchmark_bricks dir )
  include_directories( ${dir} )
  file( GLOB SRC "${dir}/brick-*[a-z]" )
  bricks_benchmark( benchmark-bricks ${SRC} )
  target_link_libraries( benchmark-bricks pthread )
endfunction()

# Run feature checks and define -DBRICKS_* feature macros. You can use bricks
# without feature checks, but you may be missing some of the features that
# way. Calling this macro from your toplevel CMakeLists.txt is therefore a good
# idea.

macro( bricks_check_dirent )
  include( CheckCXXSourceCompiles )

  check_cxx_source_compiles(
   "#include <dirent.h>
    int main() {
        struct dirent *d;
        (void)d->d_type;
        return 0;
    }" BRICKS_HAVE_DIRENT_D_TYPE )

  if ( BRICKS_HAVE_DIRENT_D_TYPE )
    add_definitions( -DBRICKS_HAVE_DIRENT_D_TYPE )
  endif()
endmacro()

macro( bricks_check_llvm )
  find_package( LLVM )
  if( LLVM_FOUND )
    add_definitions( -DBRICKS_HAVE_LLVM -isystem ${LLVM_INCLUDE_DIRS} )
    set( BRICK_USED_LLVM_LIBS ${BRICK_LLVM_LIBS} )
  endif()
endmacro()

macro( bricks_check_features )
    bricks_check_dirent()
    bricks_check_llvm()
endmacro()

function( bricks_fetch_tbb )
  include( ExternalProject )
  ExternalProject_Add(
    bricks-tbb-build
    URL https://www.threadingbuildingblocks.org/sites/default/files/software_releases/source/tbb42_20140601oss_src.tgz
    BUILD_COMMAND make
    CONFIGURE_COMMAND :
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND
     sh -c "ln -fs build/*_release _release && ln -fs build/*_debug _debug" ;
     && sh -c "cd _debug && ln -s libtbb_debug.so libtbb.so" ;
     && sh -c "cd _debug && ln -s libtbbmalloc_debug.so libtbbmalloc.so" )

  ExternalProject_Get_Property( bricks-tbb-build SOURCE_DIR BINARY_DIR )

  if ( "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" )
    set( tbb_BINARY_DIR "${BINARY_DIR}/_debug" )
  else()
    set( tbb_BINARY_DIR "${BINARY_DIR}/_release" )
  endif()

  set( tbb_SOURCE_DIR ${SOURCE_DIR} PARENT_SCOPE )
  set( tbb_BINARY_DIR ${tbb_BINARY_DIR} PARENT_SCOPE )

  add_library( bricks-tbb UNKNOWN IMPORTED )
  set_property( TARGET bricks-tbb PROPERTY IMPORTED_LOCATION
                ${tbb_BINARY_DIR}/libtbb.so )
  add_dependencies( bricks-tbb bricks-tbb-build )
endfunction()

macro( bricks_use_tbb )
  add_definitions( -DBRICKS_HAVE_TBB )
  include_directories( ${tbb_SOURCE_DIR}/include )
  link_directories( ${tbb_BINARY_DIR} )
  link_libraries( bricks-tbb )
endmacro()
