## This file contains support functions and macros for making use of bricks
## easier with cmake-based projects. Call include(bricks/support.cmake) in your
## toplevel CMakeLists.txt to access those functions.

function( update_file name content )
  if( EXISTS ${name} )
    file( READ ${name} old )
  endif()

  if( NOT "${old}" STREQUAL "${content}" )
    file( WRITE ${name} ${content} )
  endif()
endfunction()

function( bricks_make_runner name header main defs )
  set( file "${CMAKE_CURRENT_BINARY_DIR}/${name}-runner.cpp" )

  foreach( src ${ARGN} )
    set( new "${new}\n#include <${src}>" )
  endforeach( src )

  set( new "
    #include <${header}>
    ${new}
    int main( int argc, const char **argv ) {
      ${main}
      return 0\;
    }"
  )

  update_file( ${file} "${new}" )

  add_executable( ${name} EXCLUDE_FROM_ALL ${file} )
  set_source_files_properties( ${file} PROPERTIES COMPILE_DEFINITIONS ${defs} )
endfunction()

# Create a unit test driver for a bunch of header files. Syntax:
#
#    bricks_unittest( driver_name header1 header2 ... )
#
# This will get you an executable target driver_name that you can run
# to run the testsuite.

function( bricks_unittest name )
  bricks_make_runner( ${name} "brick-unittest.h" "
      return brick::unittest::run( argc > 1 ? argv[1] : \"\",
                                   argc > 2 ? argv[2] : \"\" )\;"
      "BRICK_UNITTEST_REG" ${ARGN} )
  set_target_properties( ${name} PROPERTIES COMPILE_FLAGS "-UNDEBUG" )
endfunction()

function( bricks_benchmark name )
  bricks_make_runner( ${name} "brick-benchmark.h" "brick::benchmark::run( argc, argv )\;"
                      "BRICK_BENCHMARK_REG" ${ARGN} )
  target_link_libraries( ${name} rt )
endfunction()

# Register a target "test-bricks" that builds and runs all the unit tests
# included with bricks. Use test_bricks( directory_with_bricks ). Also note
# that if you write your own unit tests using brick-unittesh.h, the tests of
# any bricks that you use in the tested units will be automatically included in
# your testsuite.

function( test_bricks dir )
  include_directories( ${dir} )
  file( GLOB SRC "${dir}/brick-*.h" )
  bricks_unittest( test-bricks ${SRC} )
  target_link_libraries( test-bricks pthread )
endfunction()

function( benchmark_bricks dir )
  include_directories( ${dir} )
  file( GLOB SRC "${dir}/brick-*.h" )
  bricks_benchmark( benchmark-bricks ${SRC} )
  target_link_libraries( benchmark-bricks pthread )
endfunction()

# Run feature checks and define -DBRICKS_* feature macros. You can use bricks
# without feature checks, but you may be missing some of the features that
# way. Calling this macro from your toplevel CMakeLists.txt is therefore a good
# idea.

macro( bricks_check_features )
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

macro( brick_find_prereq )
    # on some mingw32, regex.h is not on the default include path
    find_path( RX_PATH regex.h )
    if( RX_PATH )
      include_directories( ${RX_PATH} )
    endif()
endmacro()
