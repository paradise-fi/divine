cmake_minimum_required( VERSION 3.2 )

# this must be set BEFORE project
SET( CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -DNVALGRIND" CACHE STRING "" )
SET( CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -DNVALGRIND" CACHE STRING "" )

SET( CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG -DNVALGRIND" CACHE STRING "" )
SET( CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -g -DNDEBUG -DNVALGRIND" CACHE STRING "" )

SET( CMAKE_CXX_FLAGS_RELWITHDEBUG "-O3 -g" CACHE STRING
    "Flags used by the compiler during RelWithDebug builds. " )
SET( CMAKE_C_FLAGS_RELWITHDEBUG "-O3 -g" CACHE STRING
    "Flags used by the compiler during RelWithDebug builds. " )

SET( CMAKE_CXX_FLAGS_RELWITHASSERT "-O3 -DNVALGRIND" CACHE STRING
    "Flags used by the compiler during RelWithAssert builds. " )
SET( CMAKE_C_FLAGS_RELWITHASSERT "-O3 -DNVALGRIND" CACHE STRING
    "Flags used by the compiler during RelWithAssert builds. " )

project( divine )
enable_language( CXX )

set( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/release/cmake" )

mark_as_advanced( FORCE CMAKE_CXX_FLAGS_RELWITHDEBUG CMAKE_C_FLAGS_RELWITHDEBUG
    CMAKE_CXX_FLAGS_RELWITHASSERT CMAKE_C_FLAGS_RELWITHASSERT )

if(NOT CMAKE_BUILD_TYPE)
 set(CMAKE_BUILD_TYPE "Release" CACHE STRING
     "Choose the type of build, options are: Debug Release RelWithDebInfo RelWithDebug RelWithAssert MinSizeRel."
     FORCE)
endif()

macro(appendFlag flags value)
  foreach( F ${flags} )
    set( CMAKE_${F}_FLAGS "${CMAKE_${F}_FLAGS} ${value}" )
  endforeach( F )
endmacro(appendFlag)

macro( disable name detail reason )
    list( FIND OPTIONS ${name} opt_idx )
    if( opt_idx LESS 0 )
        message( FATAL_ERROR "INTERNAL ERROR: tried to disable non-existent option ${name}" )
    endif()
    if( ${${name}} )
        message( "-- WARNING: ${name}${detail} DISABLED. ${reason}" )
        set( DISABLED "${DISABLED} ${name}" )
        set( ${name} OFF )
    endif()
endmacro()

macro( opt name help defval )
    option( ${opt} "${name}" "${help}" "${defval}" )
    list( APPEND OPTIONS ${name} )
endmacro()

macro( with_warnings )
    appendFlag( "CXX;C" "-Wall" )
    appendFlag( "CXX" "-Wold-style-cast -Wno-unused-function" )
    if( DEV_WARNINGS )
        appendFlag( "CXX;C" "-Wextra" )
    endif()
endmacro()

# some build-type-dependent defaults

if( CMAKE_BUILD_TYPE STREQUAL "Debug" )
    set( DEV_WARNINGS_DEF ON )
    set( DEV_CONFLATE_DEF ON )
else()
    set( DEV_WARNINGS_DEF OFF )
    set( DEV_CONFLATE_DEF OFF )
endif()

############################################
## BUILD OPTIONS
##

opt( OPT_SQL "enable ODBC-based database reports" ON )
opt( OPT_TBBMALLOC "use Intel TBB memory allocator" ON )

opt( ALG_MAP "" OFF )
opt( ALG_OWCTY "" ON )
opt( ALG_REACHABILITY "" ON )
opt( ALG_WEAKREACHABILITY "" OFF )
opt( ALG_EXPLICIT "" OFF )
opt( ALG_CSDR "" ON )
opt( ALG_NDFS "" OFF )
opt( ALG_METRICS "" ON )

opt( DEV_NOPOOLS "avoid pool allocation for graph vertices" OFF )
opt( DEV_WARNINGS "enable extra compiler warnings" ${DEV_WARNINGS_DEF} )

option( BUILD_SHARED_LIBS "build shared libraries for libdivine and libdivine-instances" OFF )

include( VersionAppend )

include( FindLLVM )
include( FindPerl )

###############################################
## MISC OPTION IMPLEMENTATION
##

if( CMAKE_CXX_COMPILER_ID STREQUAL "GNU" )
    if ( WIN32 )
        if( CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.8" )
            message( FATAL_ERROR "You need at least gcc 4.8 to compile DIVINE on Windows" )
        endif()
    else()
        if( CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.7" )
            message( FATAL_ERROR "You need at least gcc 4.7 to compile DIVINE" )
        elseif( CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.8" )
            # this is nasty hack to make divine compile with gcc which was not
            # configured with --enable-libstdcxx-time
            add_definitions( -D_GLIBCXX_USE_NANOSLEEP -D_GLIBCXX_USE_SCHED_YIELD )
        endif()
    endif()
endif()

# clang doesn't print colored diagnostics when invoked from Ninja
if (UNIX AND
    CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
    CMAKE_GENERATOR STREQUAL "Ninja")
  appendFlag( "C;CXX" "-fcolor-diagnostics" )
endif()

include( CheckFunctionExists )
check_function_exists( posix_fallocate HAVE_POSIX_FALLOCATE )
if ( HAVE_POSIX_FALLOCATE )
    add_definitions( -DHAVE_POSIX_FALLOCATE )
endif()

find_program( SHA1SUM NAMES sha1sum sha1 )

if( NOT SHA1SUM )
  message( "Did not find sha1sum, version will be imprecise." )
endif( NOT SHA1SUM )

#########################################
## OPTIONAL FEATURES
##

include( bricks/support.cmake )
bricks_check_features()
include_directories( bricks )

find_package( PkgConfig )
pkg_check_modules( VALGRIND valgrind )

if ( VALGRIND_FOUND )
    include_directories( ${VALGRIND_INCLUDEDIR} )
else()
    message( WARNING "Valgrind (headers) not found! Pool debugging will not work." )
    add_definitions( -DNVALGRIND )
endif()

find_package( Curses )
if( CURSES_FOUND )
    include_directories( ${CURSES_INCLUDE_PATH} )
else()
    message( SEND_ERROR "Curses not found." )
endif()

if( NOT LLVM_FOUND )
    message( SEND_ERROR "LLVM not found." )
endif()

if ( NOT ${LLVM_STRING_VERSION} MATCHES "3[.][7]([^0-9]*|[.].*)" )
    message( SEND_ERROR "LLVM version ${LLVM_STRING_VERSION} does not match 3.7." )
endif()

link_directories( ${LLVM_LIBRARY_DIRS} )
# this is not very nice but it works and ncurses & zlib break builds under NixOS
list( REMOVE_ITEM LLVM_SYSLIBS "z" "ncurses" )
# suppress warnings coming from LLVM headers
add_definitions( -isystem ${LLVM_INCLUDE_DIRS} )

find_program( PANDOC pandoc )
find_program( PDFLATEX pdflatex )
find_program( BIBTEX bibtex )
if ( PANDOC )
    if ( NOT PDFLATEX OR NOT BIBTEX )
        message( WARNING "pdflatex or bibtex not found. Skipping PDF generation." )
    endif()
    add_subdirectory( manual )
    add_subdirectory( website )
endif()

if( OPT_SQL )
  include( FindODBC )
  if( NOT ODBC_FOUND )
    disable( OPT_SQL "" "Could not find ODBC." )
  else()
      add_definitions( "-DNANODBC_USE_CPP11" )
      link_directories( ${ODBC_LIBRARY_DIRS} )
  endif()
endif()

set( DIVINE_INCLUDES ${divine_SOURCE_DIR} ${divine_BINARY_DIR} )

foreach( opt ${OPTIONS} )
    if ( ${${opt}} )
        add_definitions( "-D${opt}=1" )
        set( BUILDOPTS "${BUILDOPTS} ${opt}" )
    else()
        add_definitions( "-D${opt}=0" )
    endif()
endforeach()

file( READ release/version DIVINE_MAJOR )
file( READ release/patchlevel PATCHLEVEL )
string( STRIP "${DIVINE_MAJOR}" DIVINE_MAJOR )
string( STRIP "${PATCHLEVEL}" PATCHLEVEL )
set( DIVINE_VERSION "${DIVINE_MAJOR}.${PATCHLEVEL}" )
# set( TEST_WRAPPER "${CMAKE_SOURCE_DIR}/nix/test-wrapper.sh" )
set( TEST_WRAPPER "-c" )

##########################################
## SOURCE DIRECTORIES
##

add_custom_target( check )
add_custom_target( unit )

if( OPT_TBBMALLOC )
  add_subdirectory( external/tbbmalloc )
endif()

add_subdirectory( external/pdclib )
add_subdirectory( external/libm )
add_subdirectory( external/libcxxabi )
add_subdirectory( external/libcxx )
add_subdirectory( runtime )

add_subdirectory( divine )
add_subdirectory( tools )

if( OPT_SQL )
    add_subdirectory( external/nanodbc )
endif()

###############################################
## TESTING
##

add_dependencies( check unit )
add_dependencies( check functional )

add_subdirectory( test )

###############################################
## MAKE INSTALL
##

include( GNUInstallDirs )

if( WIN32 )
  install(FILES README RENAME README.txt DESTINATION ./ COMPONENT sys)
  install(FILES COPYING RENAME COPYING.txt DESTINATION ./ COMPONENT sys)
  install(FILES AUTHORS RENAME AUTHORS.txt DESTINATION ./ COMPONENT sys)
  install(FILES NEWS RENAME NEWS.txt DESTINATION ./ COMPONENT sys)
else()
  install( FILES README COPYING AUTHORS NEWS DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT doc )
endif()

if( WIN32 )
  macro( bundle var name )
    find_file( ${var} ${name} )
    if( ${var} )
      install( FILES ${${var}} DESTINATION ./ COMPONENT sys)
    endif()
  endmacro()

  bundle( GNURX_LIB libgnurx-0.dll )
  bundle( REGEX_LIB msys-regex-1.dll )
  bundle( GCC_SJL_LIB libgcc_s_sjlj-1.dll )
  bundle( GCC_DW2_LIB libgcc_s_dw2-1.dll )
  bundle( GCC_STDCPP libstdc++-6.dll )
  bundle( WINPTHREAD_LIB libwinpthread-1.dll )
  bundle( MINGW_LIB mingwm10.dll )
  bundle( MSYS_LIB msys-1.0.dll )
endif( WIN32 )

########################################
## BINARY INSTALLER
##

set( CPACK_PACKAGE_VENDOR "ParaDiSe Laboratory" )
set( CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README" )
set( CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING" )
set( CPACK_PACKAGE_VERSION ${DIVINE_VERSION} )

macro(versionExtract out n in)
   string(REGEX REPLACE "([0-9]*)\\.([0-9]*)\\.?([0-9]*)?" "\\${n}" ${out} ${in})
endmacro()

versionExtract( CPACK_PACKAGE_VERSION_MAJOR 1 ${DIVINE_VERSION} )
versionExtract( CPACK_PACKAGE_VERSION_MINOR 2 ${DIVINE_VERSION} )
versionExtract( CPACK_PACKAGE_VERSION_PATCH 3 ${DIVINE_VERSION} )

message( "## -----------------------------------------------------------------" )
message( "## Version: ${DIVINE_VERSION}" )
message( "## Build type: ${CMAKE_BUILD_TYPE}" )
message( "## Build options: ${BUILDOPTS}" )
if ( DISABLED )
    message( "## Requested but *disabled*:${DISABLED}" )
endif()
message( "## -----------------------------------------------------------------" )

if ( NOT ( "${REQUIRED}" STREQUAL "" ) )
    foreach ( i ${REQUIRED} )
        if ( NOT ( "${BUILDOPTS}" MATCHES "${i}"  ))
            message( FATAL_ERROR "FATAL: ${i} is disabled but it is required by user, exiting." )
        endif()
    endforeach()
endif()

set( CPACK_SOURCE_PACKAGE_FILE_NAME
      "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION}${VERSION_APPEND}" )

set( CPACK_SOURCE_IGNORE_FILES "/build/;/_build/;/_darcs/;~$;${CPACK_SOURCE_IGNORE_FILES}" )

include( CPack )

test_bricks( bricks )
benchmark_bricks( bricks )
