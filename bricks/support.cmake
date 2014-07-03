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

# Create a unit test driver for a bunch of header files. Syntax:
#
#    bricks_unittest( driver_name header1 header2 ... )
#
# This will get you an executable target driver_name that you can run
# to run the testsuite.

function( bricks_unittest name )
  set( main "${CMAKE_CURRENT_BINARY_DIR}/${name}-main.cpp" )

  foreach( src ${ARGN} )
    set( new "${new}\n#include <${src}>" )
  endforeach( src )

  set( new "${new}
    int main( int argc, const char **argv ) {
      brick::unittest::run( argc > 1 ? argv[1] : \"\",
                            argc > 2 ? argv[2] : \"\" )\;
    }"
  )

  update_file( ${main} "${new}" )

  add_executable( ${name} ${main} )
  set_source_files_properties( ${main} PROPERTIES COMPILE_DEFINITIONS UNITTEST )
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
