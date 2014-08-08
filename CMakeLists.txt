cmake_minimum_required(VERSION 2.8.0)

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

SET( REQUIRED "" CACHE STRING "Required components (if they are disabled, configure will fail)." )

project( divine )

set( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake" )

if(COMMAND cmake_policy)
  cmake_policy(SET CMP0002 OLD) # FIXME
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

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

macro( disabling name detail reason )
    message( "-- WARNING: ${name}${detail} DISABLED. ${reason}" )
    set( DISABLED "${DISABLED} ${name}" )
endmacro()

macro( with_warnings )
    appendFlag( "CXX;C" "-Wall" )
    if( WARNINGS )
        appendFlag( "CXX;C" "-Wextra" )
    endif()
endmacro()

############################################
## USER-LEVEL OPTIONS
##

option( MPI "compile DiVinE with MPI for clustering support" ON )
option( LLVM "compile DiVinE with LLVM bitcode verification support" ON )
option( DVE "include the DVE interpreter in the build" ON )
option( COIN "include the CoIn interpreter in the build" ON )
option( CESMI "include the CESMI binary interface in the build" ON )
option( TIMED "include timed automata interpreter in the build" ON )
option( LTL3BA "use ltl3ba for LTL to BA translation" OFF )
option( MURPHI "enable Murphi model compiler" ON )
option( CURSES "link to curses library for nice statistics" ON )
option( HASH_COMPACTION "enable Hash Compaction" OFF )
option( COMPRESSION "enable lossless compression" ON )
option( EXPLICIT "include explicit state space representation support (gen-explicit algorithm and explicit generator)" ON )

option( MANUAL "build pdf/html manual" ON )
option( GUI "compile the Qt GUI" ON )
option( LTL2DSTAR "build LTL2DSTAR, for LTL support in probabilistic MC" OFF )
option( HOARD "use HOARD SMP memory allocator" OFF )
option( TBBMALLOC "use Intel TBB memory allocator" ON )

option( MPIDEBUG "make MPI code show lots of random debug noise" OFF )

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set( WARNINGS_DEF ON )
else()
    set( WARNINGS_DEF OFF )
endif()
option( WARNINGS "enable extra compiler warnings" ${WARNINGS_DEF} )

option( USE64 "pass -m64 to gcc to force 64b on multiarch" OFF )
option( USE32 "pass -m32 to gcc to force 32b on multiarch" OFF )
include (VersionAppend)

option( SQL_REPORT "Include feature for reporting into database (requires ODBC)" ON )

############################################
## DEVELOPER-ORIENTED OPTIONS
##

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set( PERFORMANCE_DEF OFF )
else()
    set( PERFORMANCE_DEF ON )
endif()

option( PERFORMANCE "compile for maximum performance (LONG compilation)" ${PERFORMANCE_DEF} )
option( PROFILE "generate profiling information for lcov coverage" OFF )
option( POOLS "use pool allocation for graph vertices" ON )
option( SMALL "reduce size of the binary, disabling optional features" OFF )
option( DIVINE_BINUTILS "build binutils for divine (enable if your binutils does not support plugins and you want LLVM verification" OFF )

set( MAX_INSTANCE_FILES_COUNT "128" CACHE STRING
    "maximal number of used instance files to generate -- more means less memory requirements" )
set( MIN_INSTANCES_PER_FILE "10" CACHE STRING
    "minimal number of instances in one instance file" )
mark_as_advanced( MAX_INSTANCE_FILES_COUNT MIN_INSTANCES_PER_FILE )

set( DEFAULT_AR "ar" )
set( DEFAULT_GOLD "ld.gold" )

if( DIVINE_BINUTILS )
    set( DEFAULT_AR "divine-ar" )
    set( DEFAULT_GOLD "divine-ld.gold" )
endif()

set( CMD_GCC "gcc" CACHE STRING "how to run the GCC C/C++ compiler" )
set( CMD_CLANG "clang" CACHE STRING "how to run the CLang C/C++ compiler" )
set( CMD_GOLD ${DEFAULT_GOLD} CACHE STRING "how to run the GNU gold linker" )
set( CMD_AR ${DEFAULT_AR} CACHE STRING "how to run GNU ar w/ gold plugin support" )

include( FindLLVM )

find_file( CMD_LLVMGOLD "LLVMgold.so"
    HINTS "${LLVM_LIBRARY_DIRS}"
    PATHS
        "/usr/lib/"
        "/usr/lib/bfd-plugins/"
        "/usr/lib/llvm/"
        "/usr/lib/llvm-3.2/"
        "/usr/lib/llvm-3.2/lib/"
        "/usr/lib/llvm-3.1/"
        "/usr/lib/llvm-3.1/lib/"
        "/usr/lib/llvm-3.3/"
        "/usr/lib/llvm-3.3/lib/"
        "/usr/lib/llvm-3.4/"
        "/usr/lib/llvm-3.4/lib/"
        "$ENV{HOME}/.nix-profile/lib"
        "/run/current-system/sw/lib/"
    )
set( CMD_LLVMGOLD "/usr/lib/llvm/LLVMgold.so" CACHE STRING "path to the LLVMgold.so plugin" )

###############################################
## MISC OPTION IMPLEMENTATION
##

if( WIN32 )
  # PE Executables have fixed stack size and the BISON parser needs a big one
  appendFlag( "EXE_LINKER;SHARED_LINKER" "-Wl,--stack,10485760" )
else()
  add_definitions( -DPOSIX )
endif( WIN32 )

if( SMALL )
  add_definitions( -DO_SMALL )
endif( SMALL )

if( COIN )
  add_definitions( -DO_COIN )
endif( COIN )

if( CESMI )
    add_definitions( -DO_CESMI )
endif( CESMI )

# on some mingw32, regex.h is not on the default include path
find_path( RX_PATH regex.h )
if ( RX_PATH )
  include_directories( ${RX_PATH} )
  link_directories( ${RX_PATH} )
endif()

if( PERFORMANCE )
  if ( NOT SMALL )
    add_definitions( -DO_PERFORMANCE )
  else()
    disabling( "PERFORMANCE" "" "Overriden by SMALL." )
  endif()
endif( PERFORMANCE )

if( HOARD AND TBBMALLOC )
    disabling( "HOARD" "" "Overriden by TBBMALLOC." )
    set( HOARD OFF )
endif()

if( NOT POOLS AND COMPRESSION )
    disabling( "COMPRESSION" "" "It is not supported without POOLS" )
    set( COMPRESSION OFF )
endif()

if ( NOT POOLS AND MPI )
    disabling( "MPI" " (cluster support)" "It is not supported without POOLS" )
    set( MPI OFF )
endif()

include( CheckCXXSourceCompiles )
set( CMAKE_REQUIRED_FLAGS_SAVE ${CMAKE_REQUIRED_FLAGS} )
set( CMAKE_REQUIRED_FLAGS "-std=c++11" ) # for the benefit of GCC

check_cxx_source_compiles("
template< typename F > int check( F f );

template< typename T, typename F, int n >
auto findID_helper( F f ) ->
    decltype( check< typename T::template X< n, true >::Fun >( f ) )
{}
int main() { return 0; }
" HAVE_CXX11 )

if( HAVE_CXX11 )
  check_cxx_source_compiles("
      struct ConstructorTest {
          ConstructorTest(int) {}
      };

      union UnionTest {
          ConstructorTest b;
      };

      int main() { return 0; }
  " HAVE_CXX11_UNRESTRICTED_UNIONS )

  check_cxx_source_compiles("
      #include <atomic>

      int main() {
          std::atomic< int > aint( 0 );
          int a( 0 ), b( 1 );
          aint.compare_exchange_strong( a, b );
          aint++;
          aint |= 1;
          aint.exchange( 0 );
          aint.compare_exchange_weak( a, b );
          return 0;
      }
  " HAVE_CXX11_ATOMIC )

  check_cxx_source_compiles("
      #include <type_traits>

      enum class Enum : unsigned { A, B, C };

      template< typename E >
      class A {
          static_assert( std::is_same< typename std::underlying_type< E >::type, unsigned >::value, \"\" );
      };

      int main() {
          A< Enum > e;
          return 0;
      }
  " HAVE_CXX11_TRAITS )
endif()

set( CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS_SAVE} )
if( WIN32 ) # strdup does not exist with c++11 on win32
    appendFlag( "CXX" "-std=gnu++11" )
else()
    appendFlag( "CXX" "-std=c++11" )
endif()

if( NOT HAVE_CXX11 )
    message( FATAL_ERROR "DiVinE requires a C++11 compliant compiler." )
endif()

if( NOT HAVE_CXX11_ATOMIC )
    message( FATAL_ERROR "DiVinE requires working C++11 std::atomic." )
endif()

if ( NOT HAVE_CXX11_TRAITS )
    message( FATAL_ERROR "DiVinE requires working C++11 type traits." )
endif()

if( DVE )
  if( HAVE_CXX11_UNRESTRICTED_UNIONS )
    add_definitions( -DO_DVE )
  else()
    message( "DVE" "" "C++11 unrestricted unions not available." )
  endif()
endif()

if( HASH_COMPACTION )
    add_definitions( -DO_HASH_COMPACTION )
endif()

if( COMPRESSION )
    add_definitions( -DO_COMPRESSION )
endif()

if( EXPLICIT )
    if ( WIN32 )
        message( "EXPLICIT" "" "is currently disabled on Windows." )
    else()
        add_definitions( -DO_EXPLICIT )
    endif()
endif()

if( MPIDEBUG )
    add_definitions( -DO_MPIDEBUG )
endif()

if( PROFILE )
  appendFlag( "CXX;C" "-fprofile-arcs -ftest-coverage" )
  link_libraries( "gcov" )
endif( PROFILE )

if( USE64 )
  appendFlag( "CXX;C;EXE_LINKER;SHARED_LINKER" "-m64" )
  add_definitions( -DO_USE_GCC_M64 )
endif( USE64 )

if( USE32 )
  appendFlag( "CXX;C;EXE_LINKER;SHARED_LINKER" "-m32" )
  add_definitions( -DO_USE_GCC_M32 )
endif( USE32 )

if( POOLS )
  add_definitions( "-DO_POOLS" )
endif( POOLS )

find_program( SHA1SUM sha1sum )

if( NOT SHA1SUM )
  message( "I could use a working sha1sum program for the build. Please install one or supply me with a path if you already have it. Thank you." )
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
    message( "-- WARNING: Valgrind (headers) not found! Pool debugging will not work." )
    add_definitions( -DNVALGRIND )
endif()

if( CURSES AND NOT USE32 AND NOT USE64 )
  # the curses package we'd find probably wouldn't work anyway
  find_package( Curses )
  if( CURSES_FOUND )
    add_definitions( -DO_CURSES )
    include_directories( ${CURSES_INCLUDE_PATH} )
  else()
    set( CURSES OFF )
  endif()
endif()

if ( TIMED )
  find_package( Boost )
  find_package( LibXml2 )
  if( NOT LIBXML2_FOUND )
    disabling( "TIMED" " (timed automata)" "Could not find LibXml2." )
    set( TIMED OFF )
  elseif( NOT Boost_FOUND )
    disabling( "TIMED" " (timed automata)" "Could not find Boost." )
    set( TIMED OFF )
  else()
    include_directories( ${LIBXML2_INCLUDE_DIR} )
    link_directories( ${PC_LIBXML_LIBDIR} )
    list( APPEND ADDITIONAL_LIBS ${LIBXML2_LIBRARIES} )
    add_definitions( -DO_TIMED )
  endif()
endif()

if( LLVM )
  if( NOT LLVM_FOUND )
    disabling( "LLVM" "" "Could not find LLVM." )
    set( LLVM OFF )
  else()
    if ( ${LLVM_STRING_VERSION} MATCHES "3[.][1234]([^0-9]*|[.].*)" )
      add_definitions( -DO_LLVM )
      link_directories( ${LLVM_LIBRARY_DIRS} )

      # this is not very nice but it works and ncurses & zlib break builds under NixOS
      list( REMOVE_ITEM LLVM_SYSLIBS "z" "ncurses" )
    else()
      disabling( "LLVM" "" "Found version ${LLVM_STRING_VERSION} but 3.1 to 3.4 is required." )
      set( LLVM OFF )
    endif()
  endif()
endif()

if( MANUAL )
  find_program( PANDOC pandoc )
  find_program( PDFLATEX pdflatex )
  find_program( BIBTEX bibtex )
  if ( PANDOC )
    if ( NOT PDFLATEX OR NOT BIBTEX )
      message( "-- WARNING: pdflatex or bibtex not found. Skipping PDF generation." )
    endif()
  else()
    disabling( "MANUAL" " (build pdf/html documentation)" "Could not find pandoc.")
    set( MANUAL OFF )
  endif()
endif()

if( MPI AND NOT WIN32 ) # MPI not supported on win32
  include( FindMPI )
  if ( MPI_FOUND )
    add_definitions( "-DO_MPI" )
    foreach( path ${MPI_INCLUDE_PATH} )
      set( MPI_INCLUDE_FLAGS "-I${path} ${MPI_INCLUDE_FLAGS}" )
    endforeach()
    include (CheckIncludeFileCXX)
    check_include_file_cxx( "ompi/version.h" HAVE_OMPI_VERSION ${MPI_INCLUDE_FLAGS} )
    if( HAVE_OMPI_VERSION )
      add_definitions( "-DO_OMPI_VERSION" )
    endif()
  else()
    disabling( "MPI" " (cluster support)" "Could not find MPI." )
    set( MPI OFF )
    set( MPI_INCLUDE_PATH "" )
    set( MPI_LIBRARIES "" )
    set( MPI_LIBRARY "" )
    set( MPIEXEC "" )
  endif()
endif()

if( PROFILE )
  find_program( LCOV lcov )
  find_program( GCOV gcov )
  find_program( LCOV_GENHTML genhtml )

  if( LCOV )
    set( LCOV_BIN "${LCOV}" )
    set( GCOV_BIN "${GCOV}" )
    set( LCOV_IN "${divine_BINARY_DIR}" )
    set( LCOV_OUT "${divine_BINARY_DIR}/lcov-report" )
    configure_file( cmake/with-lcov.sh ${divine_BINARY_DIR}/with-lcov )
    execute_process( COMMAND chmod +x ${divine_BINARY_DIR}/with-lcov )
    set( WIBBLE_WRAP_TESTS "${divine_BINARY_DIR}/with-lcov" )

    add_custom_target( lcov-report
      COMMAND mkdir -p ${LCOV_OUT}
      COMMAND ${LCOV} --quiet -b ${divine_SOURCE_DIR} -d ${divine_SOURCE_DIR}
                      -a ${LCOV_OUT}/base.info
                      -a ${LCOV_OUT}/collect-1.info
                      -a ${LCOV_OUT}/collect-2.info
                      -a ${LCOV_OUT}/collect-3.info
                      -o ${LCOV_OUT}/overall_everything.info
      COMMAND ${LCOV} --extract ${LCOV_OUT}/overall_everything.info
                      ${divine_SOURCE_DIR}/divine/*
                      ${divine_SOURCE_DIR}/examples/*
                      ${divine_SOURCE_DIR}/murphi/*
                      ${divine_SOURCE_DIR}/tools/*
                      -o ${LCOV_OUT}/overall.info
      COMMAND ${LCOV_GENHTML} -o lcov-report -p `pwd` -p ${divine_SOURCE_DIR}
        ${LCOV_OUT}/overall.info
      VERBATIM
    )
  else()
    disabling( "PROFILE" " (coverage report generation)" "Could not find LCOV." )
    set( PROFILE OFF )
  endif()
endif()

if ( LTL3BA )
  add_definitions( -DO_LTL3BA )
endif()

if( LTL2DSTAR )
  find_package( Boost )
  if( Boost_FOUND )
    add_definitions( -DO_LTL2DSTAR )
  else()
    set( LTL2DSTAR OFF )
  endif()
endif()

if( MURPHI )
  find_program( BYACC "byacc" )
  find_package( FLEX )
  find_package( BYACC )
  if ( FLEX_FOUND AND BYACC_FOUND AND BYACC )
    add_definitions( -DO_MURPHI )
  else()
    disabling( "MURPHI" "" "Could not find flex and/or byacc" )
    set( MURPHI OFF )
  endif()
endif()

if( GUI )
  find_package( Qt4 4.5.0 )
  if( QT4_FOUND AND QT_VERSION_MINOR GREATER 4 )
    if ( WIN32 )
      add_definitions( -DQT_EDITION=QT_EDITION_OPENSOURCE )
    endif()

    if( NOT QT_QCOLLECTIONGENERATOR_EXECUTABLE )
        find_program( QT_QCOLLECTIONGENERATOR_EXECUTABLE qcollectiongenerator )
        if( NOT QT_QCOLLECTIONGENERATOR_EXECUTABLE )
            message( "-- WARNING: qcollectiongenerator not found." )
        endif()
    endif()
  else()
    disabling( "GUI" "" "Could not find Qt4 >= 4.5." )
    set( GUI OFF )
  endif()
endif( GUI )

if( SQL_REPORT )
  include( FindODBC )
  if( NOT ODBC_FOUND )
    disabling( "SQL_REPORT" "" "Could not find ODBC." )
    set( SQL_REPORT OFF )
  else()
      add_definitions( -DO_SQL_REPORT )
      add_definitions( "-DNANODBC_USE_CPP11" )
      link_directories( ${ODBC_LIBRARY_DIRS} )
  endif()
endif()

if( MANUAL )
  find_program( PANDOC pandoc )
  find_program( PDFLATEX pdflatex )
  find_program( BIBTEX bibtex )
  if ( PANDOC )
    if ( NOT PDFLATEX OR NOT BIBTEX )
      message( "-- WARNING: pdflatex or bibtex not found. Skipping PDF generation." )
    endif()
  else()
    disabling( "MANUAL" " (build pdf/html documentation)" "Could not find pandoc.")
    set( MANUAL OFF )
  endif()
endif()


set( DIVINE_INCLUDES
  ${divine_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/external/ltl3ba/buddy/src
  ${CMAKE_CURRENT_SOURCE_DIR}/external/libutap/src
  ${CMAKE_CURRENT_SOURCE_DIR}/external/dbm/include
  ${divine_BINARY_DIR}
  ${MPI_INCLUDE_PATH}
  ${LLVM_INCLUDE_DIRS}
  ${divine_SOURCE_DIR}/divine/legacy )

appendFlag( "CXX" "-Wold-style-cast" )

if ( MPI )
    foreach ( path ${MPI_INCLUDE_PATH} )
        add_definitions( -isystem ${path} )
    endforeach()
endif( MPI )

if ( LLVM )
    # suppress warnings coming from LLVM headers
    add_definitions( -isystem ${LLVM_INCLUDE_DIRS} )
endif( LLVM )

get_property( BUILDOPTS1 DIRECTORY PROPERTY COMPILE_DEFINITIONS )
string(REPLACE ";" " " BUILDOPTS2 "${BUILDOPTS1}")
string(REPLACE " NANODBC_USE_CPP11" "" BUILDOPTS3 "${BUILDOPTS2}")
string(REPLACE "O_" "" BUILDOPTS "${BUILDOPTS3}")

##########################################
## SOURCE DIRECTORIES
##

add_custom_target( check )
add_custom_target( unit )

add_subdirectory( wibble )
# add_subdirectory( external/lpsolve )

if( HOARD )
  add_subdirectory( external/hoard )
endif( HOARD )

if( TBBMALLOC )
  add_subdirectory( external/tbbmalloc )
endif()

if( LTL2DSTAR )
  if ( NOT LTL3BA )
    add_subdirectory( external/ltl2ba )
  endif()
  add_subdirectory( external/ltl2dstar )
endif()

if( LLVM )
  add_subdirectory( external/pdclib )
  add_subdirectory( external/libm )
  add_subdirectory( external/libcxxabi )
  add_subdirectory( external/libcxx )
endif()

if( TIMED )
  add_subdirectory( external/libutap )
  add_subdirectory( external/dbm )
endif()

if( MURPHI )
  add_subdirectory( external/murphi )
endif()

add_subdirectory( divine )
add_subdirectory( tools )
add_subdirectory( examples )

if ( LTL3BA )
  add_subdirectory( external/ltl3ba )
endif()

if( GUI )
  add_subdirectory( gui )
endif( GUI )

if( MANUAL )
  add_subdirectory( manual )
  add_subdirectory( website )
endif()

if( SQL_REPORT )
    add_subdirectory( external/nanodbc )
endif()

if( DIVINE_BINUTILS )
    add_subdirectory( external/binutils )
    add_dependencies( divine binutils )
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

if( DIVINE_BINUTILS )
    install( PROGRAMS
        ${CMAKE_BINARY_DIR}/binutils/bin/divine-ar
        ${CMAKE_BINARY_DIR}/binutils/bin/divine-ld.gold
        DESTINATION ${CMAKE_INSTALL_BINDIR} )
endif()

########################################
## BINARY INSTALLER
##

set( CPACK_PACKAGE_DESCRIPTION_SUMMARY "DiViNE Distributed and Parallel Verification Environment" )
set( CPACK_PACKAGE_VENDOR "ParaDiSe Laboratory" )
set( CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README" )
set( CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING" )

file(STRINGS divine/utility/version.cpp VERSION_LINE REGEX "#define DIVINE_VERSION \"(.*)\"")
string(REGEX REPLACE "#define DIVINE_VERSION \"([0-9.]+)\"" "\\1" VERSION_STRING ${VERSION_LINE})

macro(versionExtract out n in)
   string(REGEX REPLACE "([0-9]*)\\.([0-9]*)\\.?([0-9]*)?" "\\${n}" ${out} ${in})
endmacro()

versionExtract( CPACK_PACKAGE_VERSION_MAJOR 1 ${VERSION_STRING} )
versionExtract( CPACK_PACKAGE_VERSION_MINOR 2 ${VERSION_STRING} )
versionExtract( CPACK_PACKAGE_VERSION_PATCH 3 ${VERSION_STRING} )
set( CPACK_PACKAGE_VERSION ${VERSION_STRING} )

message( "## -----------------------------------------------------------------" )
message( "## Version: ${VERSION_STRING}" )
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

set( CPACK_PACKAGE_INSTALL_DIRECTORY
      "DiVinE ${CPACK_PACKAGE_VERSION}${VERSION_APPEND}" )
set( CPACK_SOURCE_PACKAGE_FILE_NAME
      "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION}${VERSION_APPEND}" )

if( WIN32 )
  set( CPACK_PACKAGE_FILE_NAME
      "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION}${VERSION_APPEND}-installer" )
endif( WIN32 )

set( CPACK_SOURCE_IGNORE_FILES "/build/;/_build/;/_darcs/;~$;${CPACK_SOURCE_IGNORE_FILES}" )

set( CPACK_COMPONENTS_ALL console_tools gui_tools divine_dev wibble_dev qt sys examples help)

if(WIN32)
  set(CPACK_NSIS_MENU_LINKS "./divine.ide.exe" "DiViNE IDE"
                            "./README.txt" "README"
                            "./COPYING.txt" "COPYING"
                            "./AUTHORS.txt" "AUTHORS"
                            "./NEWS.txt" "NEWS")
  set(CPACK_PACKAGE_EXECUTABLES "" "")
  set(CPACK_NSIS_MODIFY_PATH ON)
endif(WIN32)

include( CPack )

if(CMAKE_MINOR_VERSION EQUAL 6 AND CMAKE_PATCH_VERSION GREATER 0)
  set(HAVE_CMAKE_261 1)
endif()

if(CMAKE_MAJOR_VERSION GREATER 2 OR CMAKE_MINOR_VERSION GREATER 6 OR HAVE_CMAKE_261)
  cpack_add_component_group( tools DISPLAY_NAME "Tools"
                             DESCRIPTION "Installs the basic tools"
                             EXPANDED )

  cpack_add_component_group( devel DISPLAY_NAME "Development"
                             DESCRIPTION "Installs header and library files"
                             EXPANDED )

  cpack_add_component( console_tools DISPLAY_NAME "Console Tools"
                       DESCRIPTION "Console tools"
                       GROUP tools )

  cpack_add_component( gui_tools DISPLAY_NAME "IDE"
                       DESCRIPTION "Graphical environment"
                       GROUP tools )

  cpack_add_component( wibble_dev DISPLAY_NAME "Wibble"
                       DESCRIPTION "Wibble library"
                       GROUP devel )

  cpack_add_component( divine_dev DISPLAY_NAME "Divine"
                       DESCRIPTION "Divine development files"
                       DEPENDS wibble_dev
                       GROUP devel )

  cpack_add_component( help HIDDEN DEPENDS gui_tools )

  cpack_add_component( qt HIDDEN DEPENDS gui_tools )
  cpack_add_component( sys HIDDEN )

  cpack_add_component( examples DISPLAY_NAME "Examples"
                       DESCRIPTION "Sample models" )
endif()

test_bricks( bricks )
benchmark_bricks( bricks )
