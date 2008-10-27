cmake_minimum_required(VERSION 2.4.7)
project( divine )

if(COMMAND cmake_policy)
  cmake_policy(SET CMP0002 OLD) # FIXME
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

if( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE "RELEASE" )
endif( NOT CMAKE_BUILD_TYPE )

macro(appendFlag flags value)
  foreach( F ${flags} )
    set( CMAKE_${F}_FLAGS_${CMAKE_BUILD_TYPE}
      "${CMAKE_${F}_FLAGS_${CMAKE_BUILD_TYPE}} ${value}" )
  endforeach( F )
endmacro(appendFlag)

option( PROFILE "generate profiling information for lcov coverage" OFF )
option( HOARD "use HOARD SMP memory allocator" ON )
option( USE64 "pass -m64 to gcc to force 64b on multiarch" OFF )
option( USE32 "pass -m32 to gcc to force 32b on multiarch" OFF )

if( PROFILE )
  appendFlag( "CXX;C" "-fprofile-arcs -ftest-coverage" )
  link_libraries( "gcov" )
endif( PROFILE )

if( USE64 )
  appendFlag( "CXX;C;EXE_LINKER;SHARED_LINKER" "-m64" )
endif( USE64 )

if( USE32 )
  appendFlag( "CXX;C;EXE_LINKER;SHARED_LINKER" "-m32" )
endif( USE32 )

include( CheckCXXSourceCompiles )
check_cxx_source_compiles(
 "template< typename T > struct A {
	static const int x = sizeof( T );
  };
  struct B {
        A< B > a;
  };
  int main() { return 0; }" TMPL_SIZEOF_WORKS )

if( NOT TMPL_SIZEOF_WORKS )
  message( FATAL_ERROR "Your C++ compiler apparently does not support language features required by DiVinE. It is recommended that you compile with G++ 4.1 or 4.2." )
endif( NOT TMPL_SIZEOF_WORKS )

find_program( SHA1SUM sha1sum )

if( NOT SHA1SUM )
  message( FATAL_ERROR "I need a working sha1sum program for the build. Please install one or supply me with a path to one. Thank you." )
endif( NOT SHA1SUM )

if( PROFILE )
find_program( LCOV lcov )
find_program( LCOV_GENHTML genhtml )
add_custom_target( lcov-clean
  COMMAND mkdir -p lcov-report
  COMMAND ${LCOV} -d . -z
  COMMAND find divine/CMakeFiles/divine.dir/legacy/system/ -name "\\*.gcno" | xargs rm -f
  COMMAND ${LCOV} -c -i -d . -o lcov-report/base.info
)

add_custom_target( lcov-postcheck
  COMMAND mkdir -p lcov-report
  COMMAND ${LCOV} --quiet -b ${divine_SOURCE_DIR} -d . -c -o lcov-report/check.info
  COMMAND ${LCOV} --quiet -a lcov-report/base.info
                          -a lcov-report/check.info
                          -o lcov-report/overall.info
)

add_custom_target( lcov-report
  COMMAND mkdir -p lcov-report
  COMMAND ${LCOV_GENHTML} -o lcov-report -p `pwd` -p ${divine_SOURCE_DIR}
    lcov-report/overall.info
)
add_custom_target( check )
add_dependencies( check lcov-clean )
endif( PROFILE )

add_subdirectory( wibble )
if( HOARD )
add_subdirectory( hoard )
endif( HOARD )
add_subdirectory( lpsolve )
add_subdirectory( divine )
add_subdirectory( tools )

if( PROFILE )
add_dependencies( check lcov-postcheck )
endif( PROFILE )
