cmake_minimum_required(VERSION 2.4.7)
project( divine )

if(COMMAND cmake_policy)
  cmake_policy(SET CMP0002 OLD) # FIXME
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

set( CXX_FLAGS "-pipe -gstabs+" )

set( OPT_FLAGS "-O0" )
option( OPTIMIZE "override OPT_FLAGS to -O2" ON )
option( HOARD "use HOARD SMP memory allocator" ON )

if( OPTIMIZE )
set( OPT_FLAGS "-O2 -DNDEBUG -fomit-frame-pointer" )
endif( OPTIMIZE )

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

add_subdirectory( wibble )
if( HOARD )
add_subdirectory( hoard )
endif( HOARD )
add_subdirectory( lpsolve )
add_subdirectory( divine )
add_subdirectory( tools )
