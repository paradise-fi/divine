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
option( POOLS "use pool allocation for graph vertices" ON )
option( USE64 "pass -m64 to gcc to force 64b on multiarch" OFF )
option( USE32 "pass -m32 to gcc to force 32b on multiarch" OFF )
option( MPI "compile DiVinE with MPI for clustering support" ON )

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

if( NOT POOLS )
  add_definitions( "-DDISABLE_POOLS" )
endif( NOT POOLS )

include( FindMPI )

if( MPI AND MPI_FOUND )
  add_definitions( "-DHAVE_MPI" )
else( MPI AND MPI_FOUND )
  set( MPI_INCLUDE_PATH "" )
  set( MPI_LIBRARIES "" )
  set( MPI_LIBRARY "" )
  if( MPI )
    message( "-- WARNING: I could not find MPI. Disabling cluster support." )
  endif( MPI )
  set( MPI OFF )
  set( MPIEXEC "" )
endif( MPI AND MPI_FOUND )

set( DIVINE_INCLUDES
  ${divine_SOURCE_DIR}
  ${divine_BINARY_DIR}
  ${MPI_INCLUDE_PATH}
  ${divine_SOURCE_DIR}/divine/legacy )

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

add_custom_target( check )
add_custom_target( unit )
add_custom_target( lcovd-unit )

if( PROFILE )
find_program( LCOV lcov )
find_program( LCOV_GENHTML genhtml )
set( LCOV_RDIR "${divine_BINARY_DIR}/lcov-report" )
add_custom_target( lcov-clean
  COMMAND mkdir -p ${LCOV_RDIR}
  COMMAND ${LCOV} -d ${divine_SOURCE_DIR} -z
  COMMAND ${LCOV} -c -i -d ${divine_SOURCE_DIR} -o ${LCOV_RDIR}/base.info
)

macro( lcov_collect id )
add_custom_target( lcov-collect-${id}
  COMMAND mkdir -p ${LCOV_RDIR}
  COMMAND ${LCOV} --quiet -b ${divine_SOURCE_DIR} -d ${divine_SOURCE_DIR} -c
                  -o ${LCOV_RDIR}/collect-${id}.info || true
  COMMAND ${LCOV} -d ${divine_SOURCE_DIR} --zerocounters || true
)
endmacro( lcov_collect )

add_custom_target( lcov-report
  COMMAND mkdir -p ${LCOV_RDIR}
  COMMAND ${LCOV} --quiet -b ${divine_SOURCE_DIR} -d ${divine_SOURCE_DIR}
                  -a ${LCOV_RDIR}/base.info
                  -a ${LCOV_RDIR}/collect-1.info
                  -a ${LCOV_RDIR}/collect-2.info
                  -o ${LCOV_RDIR}/overall.info
  COMMAND ${LCOV_GENHTML} -o lcov-report -p `pwd` -p ${divine_SOURCE_DIR}
    ${LCOV_RDIR}/overall.info
)
add_dependencies( check lcov-clean )
endif( PROFILE )

add_subdirectory( wibble )
if( HOARD )
add_subdirectory( hoard )
endif( HOARD )
add_subdirectory( lpsolve )
add_subdirectory( divine )
add_subdirectory( tools )
add_subdirectory( examples )

add_dependencies( lcovd-unit unit )

if( PROFILE )
  lcov_collect( "1" )
  add_dependencies( lcovd-unit lcov-collect-1 )
  add_dependencies( lcov-collect-1 unit )
endif( PROFILE )

add_dependencies( check lcovd-unit )
add_dependencies( check functional )

add_subdirectory( test )
