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

opt( OPT_MPI "compile DiVinE with MPI for clustering support" ON )
opt( OPT_LTL3BA "use ltl3ba for LTL to BA translation" OFF )
opt( OPT_LTL2DSTAR "build LTL2DSTAR, for LTL support in probabilistic MC" OFF )
opt( OPT_MURPHI "enable Murphi model compiler" ON )
opt( OPT_CURSES "link to curses library for nice statistics" ON )

opt( OPT_SQL "Include feature for reporting into database (requires ODBC)" ON )
opt( OPT_MANUAL "build pdf/html manual" ON )
opt( OPT_GUI "compile the Qt GUI" ON )
opt( OPT_HOARD "use HOARD SMP memory allocator" OFF )
opt( OPT_TBBMALLOC "use Intel TBB memory allocator" ON )

opt( OPT_SIMULATE "divine simulate" ON )
opt( OPT_DRAW "divine draw" ON )

opt( GEN_LLVM "LLVM bitcode verification" ON )
opt( GEN_LLVM_PTST "Points-To verification in LLVM bitcode" OFF )
opt( GEN_LLVM_CSDR "CSDR-enabled LLVM verification (see ALG_CSDR)" ON )
opt( GEN_LLVM_PROB "probabilistic LLVM verification" OFF )
opt( GEN_DVE "include the DVE interpreter in the build" ON )
opt( GEN_COIN "include the CoIn interpreter in the build" OFF )
opt( GEN_CESMI "include the CESMI binary interface in the build" ON )
opt( GEN_TIMED "include timed automata interpreter in the build" ON )
opt( GEN_EXPLICIT "include explicit state space loading support" ON )
opt( GEN_EXPLICIT_PROB "probabilistic explicit state space loading support" OFF )
opt( GEN_DUMMY "dummy state space (for testing only)" OFF )

opt( ALG_MAP "" OFF )
opt( ALG_OWCTY "" ON )
opt( ALG_REACHABILITY "" ON )
opt( ALG_WEAKREACHABILITY "" OFF )
opt( ALG_EXPLICIT "" OFF )
opt( ALG_CSDR "" ON )
opt( ALG_NDFS "" OFF )
opt( ALG_METRICS "" ON )

opt( STORE_HC "enable Hash Compaction" OFF )
opt( STORE_COMPRESS "enable lossless compression" ON )

opt( TRANSFORM_POR  "" ON )
opt( TRANSFORM_FAIR "" ON )

opt( DEV_CONFLATE "avoid extra faster but feature-equivalent instances" ${DEV_CONFLATE_DEF} )
opt( DEV_GCOV "generate profiling information for a lcov coverage report" OFF )
opt( DEV_NOPOOLS "avoid pool allocation for graph vertices" OFF )
opt( DEV_NOPOOLEPHEMERAL "disable ephemeral memory allocation optimization" OFF )
mark_as_advanced( DEV_NOPOOLEPHEMERAL )
opt( DEV_WARNINGS "enable extra compiler warnings" ${DEV_WARNINGS_DEF} )

option( BUILD_SHARED_LIBS "build shared libraries for libdivine and libdivine-instances" OFF )

include( VersionAppend )

set( MAX_INSTANCE_FILES_COUNT "128" CACHE STRING
    "maximal number of used instance files to generate -- more means less memory requirements" )
set( MIN_INSTANCES_PER_FILE "10" CACHE STRING
    "minimal number of instances in one instance file" )
mark_as_advanced( MAX_INSTANCE_FILES_COUNT MIN_INSTANCES_PER_FILE )

set( OPT_LLVMSPLIT "Generic" CACHE STRING
    "LLVM compression splitter, one of: Generic, PerObject, PerObjectMono and Hybrid" )
mark_as_advanced( OPT_LLVMSPLIT )

set( CMD_CC "cc" CACHE STRING "how to run (at runtime) a C compiler" )
set( CMD_CXX "c++" CACHE STRING "how to run (at runtime) a C++ compiler" )
set( CMD_CLANG "clang" CACHE STRING "how to run the CLang C/C++ compiler" )

include( FindLLVM )
include( FindPerl )

include( CheckCXXSourceCompiles )
set( CMAKE_REQUIRED_FLAGS_SAVE ${CMAKE_REQUIRED_FLAGS} )
set( CMAKE_REQUIRED_FLAGS "-std=c++14" ) # for the benefit of GCC

###############################################
## MISC OPTION IMPLEMENTATION
##

if( WIN32 )
  # PE Executables have fixed stack size and the BISON parser needs a big one
  appendFlag( "EXE_LINKER;SHARED_LINKER" "-Wl,--stack,10485760" )
endif( WIN32 )

if( GEN_COIN )
  add_definitions( -DGEN_COIN )
endif()

# on some mingw32, regex.h is not on the default include path
find_path( RX_PATH regex.h )
if ( RX_PATH )
  include_directories( ${RX_PATH} )
  link_directories( ${RX_PATH} )
endif()

if( OPT_HOARD AND OPT_TBBMALLOC )
    disable( OPT_HOARD "" "Overriden by TBBMALLOC." )
endif()

if ( DEV_NOPOOLS AND OPT_MPI )
    disable( OPT_MPI " (cluster support)" "Not supported without pools" )
endif()

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
    appendFlag( "CXX" "-std=c++14" )
endif()

if( NOT APPLE )
    appendFlag( "EXE_LINKER" "-Xlinker --no-keep-memory" )
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

include( CheckFunctionExists )
check_function_exists( posix_fallocate HAVE_POSIX_FALLOCATE )
if ( HAVE_POSIX_FALLOCATE )
    add_definitions( -DHAVE_POSIX_FALLOCATE )
endif()

if( GEN_DVE AND NOT HAVE_CXX11_UNRESTRICTED_UNIONS )
    disable( GEN_DVE "" "C++11 unrestricted unions not available." )
endif()

if( DEV_GCOV )
  appendFlag( "CXX;C" "-fprofile-arcs -ftest-coverage" )
  link_libraries( "gcov" )
endif()

find_program( SHA1SUM NAMES sha1sum sha1 )

if( NOT SHA1SUM )
  message( "I could use a working sha1sum program for the build. Please install one or supply me with a path if you already have it. Thank you." )
endif( NOT SHA1SUM )

if( NOT GEN_CESMI )
    disable( "OPT_MURPHI" "" "MURPHI requires CESMI" )
endif()

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

if( OPT_CURSES )
    find_package( Curses )
    if( CURSES_FOUND )
        include_directories( ${CURSES_INCLUDE_PATH} )
    else()
        disable( OPT_CURSES "" "Curses not found." )
    endif()
endif()

if ( GEN_TIMED )
    find_package( Boost )
    find_package( LibXml2 )
    if( NOT LIBXML2_FOUND )
        disable( GEN_TIMED " (timed automata)" "Could not find LibXml2." )
    elseif( NOT Boost_FOUND )
        disable( GEN_TIMED " (timed automata)" "Could not find Boost." )
    else()
        include_directories( ${LIBXML2_INCLUDE_DIR} )
        link_directories( ${PC_LIBXML_LIBDIR} )
        list( APPEND ADDITIONAL_LIBS ${LIBXML2_LIBRARIES} )
    endif()
endif()

if( GEN_LLVM )
    if( NOT LLVM_FOUND )
        disable( GEN_LLVM "" "Could not find LLVM." )
    elseif ( ${LLVM_STRING_VERSION} MATCHES "3[.][7]([^0-9]*|[.].*)" )
        link_directories( ${LLVM_LIBRARY_DIRS} )
        # this is not very nice but it works and ncurses & zlib break builds under NixOS
        list( REMOVE_ITEM LLVM_SYSLIBS "z" "ncurses" )
        # suppress warnings coming from LLVM headers
        add_definitions( -isystem ${LLVM_INCLUDE_DIRS} )
    else()
        disable( GEN_LLVM "" "Found version ${LLVM_STRING_VERSION} but 3.7 is required." )
    endif()

    execute_process( COMMAND "${CMD_CLANG}" --version OUTPUT_VARIABLE CLANG_VERSION__ )
    string( REGEX MATCH "clang version [0-9]*[.][0-9]*" CLANG_VERSION_ "${CLANG_VERSION__}" )
    string( REPLACE "clang version " "" CLANG_VERSION "${CLANG_VERSION_}" )
    if ( "${CLANG_VERSION}" STREQUAL "" )
        # apple clang
        string( REGEX MATCH "based on LLVM [0-9]*[.][0-9]*" CLANG_VERSION_ "${CLANG_VERSION__}" )
        string( REPLACE "based on LLVM " "" CLANG_VERSION "${CLANG_VERSION_}" )
    endif()
    string( REGEX MATCH "^[0-9]*[.][0-9]" LLVM_VERSION "${LLVM_STRING_VERSION}" )
    if ( NOT "${CLANG_VERSION}" STREQUAL "${LLVM_VERSION}" )
        disable( GEN_LLVM "" "Clang version ${CLANG_VERSION} does not match LLVM version ${LLVM_VERSION}" )
    endif()
endif()

if( NOT GEN_LLVM )
    disable( GEN_LLVM_PROB "" "GEN_LLVM is required for GEN_LLVM_PROB." )
    disable( GEN_LLVM_CSDR "" "GEN_LLVM is required for GEN_LLVM_CSDR." )
    disable( GEN_LLVM_PTST "" "GEN_LLVM is required for GEN_LLVM_PTST." )
endif()

if( OPT_MANUAL )
    find_program( PANDOC pandoc )
    find_program( PDFLATEX pdflatex )
    find_program( BIBTEX bibtex )
    if ( PANDOC )
        if ( NOT PDFLATEX OR NOT BIBTEX )
            message( "-- WARNING: pdflatex or bibtex not found. Skipping PDF generation." )
        endif()
    else()
        disable( OPT_MANUAL " (build pdf/html documentation)" "Could not find pandoc.")
    endif()
endif()

if( OPT_MPI ) # MPI not supported on win32
    include( FindMPI )
    if ( MPI_FOUND )
        foreach( path ${MPI_INCLUDE_PATH} )
            set( MPI_INCLUDE_FLAGS "-I${path} ${MPI_INCLUDE_FLAGS}" )
        endforeach()
        include (CheckIncludeFileCXX)
        check_include_file_cxx( "ompi/version.h" HAVE_OMPI_VERSION ${MPI_INCLUDE_FLAGS} )
        if( HAVE_OMPI_VERSION )
            add_definitions( "-DHAVE_OMPI_VERSION" )
        endif()
        foreach ( path ${MPI_INCLUDE_PATH} )
            add_definitions( -isystem ${path} )
        endforeach()
    else()
        disable( OPT_MPI " (cluster support)" "Could not find MPI." )
        set( MPI_INCLUDE_PATH "" )
        set( MPI_LIBRARIES "" )
        set( MPI_LIBRARY "" )
        set( MPIEXEC "" )
    endif()
endif()

if( DEV_GCOV )
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
        set( WITH_LCOV "${divine_BINARY_DIR}/with-lcov" )

        add_custom_target( lcov-report
            COMMAND mkdir -p ${LCOV_OUT}
            COMMAND ${LCOV} --quiet -b ${divine_SOURCE_DIR} -d ${divine_SOURCE_DIR}
                      -a ${LCOV_OUT}/base.info
                      -a ${LCOV_OUT}/collect-1.info
                      -a ${LCOV_OUT}/collect-2.info
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
        disable( DEV_GCOV " (coverage report generation)" "Could not find LCOV." )
        set( WITH_LCOV "" )
    endif()
endif()

if( OPT_LTL2DSTAR )
    find_package( Boost )
    if( NOT Boost_FOUND )
        disable( OPT_LTL2DSTAR "" "Boost not found." )
    endif()
endif()

if( OPT_MURPHI )
    find_package( FLEX )
    find_package( BYACC )
    if ( NOT FLEX_FOUND OR NOT BYACC_FOUND )
        disable( OPT_MURPHI "" "Could not find flex and/or byacc" )
    endif()
endif()

if( OPT_GUI )
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
    disable( OPT_GUI "" "Could not find Qt4 >= 4.5." )
  endif()
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
set( DIVINE_SYS_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/external/ltl3ba/buddy/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/libutap/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/dbm/include )

foreach( opt ${OPTIONS} )
    if ( ${${opt}} )
        add_definitions( "-D${opt}=1" )
        set( BUILDOPTS "${BUILDOPTS} ${opt}" )
    else()
        add_definitions( "-D${opt}=0" )
    endif()
endforeach()

add_definitions( "-DOPT_LLVMSPLIT=${OPT_LLVMSPLIT}" )
set( BUILDOPTS "${BUILDOPTS} OPT_LLVMSPLIT=${OPT_LLVMSPLIT}" )

file( READ release/version DIVINE_MAJOR )
file( READ release/patchlevel PATCHLEVEL )
string( STRIP "${DIVINE_MAJOR}" DIVINE_MAJOR )
string( STRIP "${PATCHLEVEL}" PATCHLEVEL )
set( DIVINE_VERSION "${DIVINE_MAJOR}.${PATCHLEVEL}" )
set( TEST_WRAPPER "${CMAKE_SOURCE_DIR}/nix/test-wrapper.sh" )

##########################################
## SOURCE DIRECTORIES
##

add_custom_target( check )
add_custom_target( unit )

# add_subdirectory( external/lpsolve )

if( OPT_HOARD )
  add_subdirectory( external/hoard )
endif()

if( OPT_TBBMALLOC )
  add_subdirectory( external/tbbmalloc )
endif()

if( OPT_LTL2DSTAR )
  if ( NOT OPT_LTL3BA )
    add_subdirectory( external/ltl2ba )
  endif()
  add_subdirectory( external/ltl2dstar )
endif()

if( GEN_LLVM )
  add_subdirectory( external/pdclib )
  add_subdirectory( external/libm )
  add_subdirectory( external/libcxxabi )
  add_subdirectory( external/libcxx )
endif()

if( GEN_TIMED )
  add_subdirectory( external/libutap )
  add_subdirectory( external/dbm )
endif()

if( OPT_MURPHI )
  add_subdirectory( external/murphi )
endif()

if ( GEN_LLVM )
  add_subdirectory( lart )
endif()

add_subdirectory( divine )
add_subdirectory( tools )
add_subdirectory( examples )

if ( OPT_LTL3BA )
  add_subdirectory( external/ltl3ba )
endif()

if( OPT_GUI )
  add_subdirectory( gui )
endif()

if( OPT_MANUAL )
  add_subdirectory( manual )
  add_subdirectory( website )
endif()

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

set( CPACK_PACKAGE_DESCRIPTION_SUMMARY "DiViNE Distributed and Parallel Verification Environment" )
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

set( CPACK_PACKAGE_INSTALL_DIRECTORY
      "DIVINE ${CPACK_PACKAGE_VERSION}${VERSION_APPEND}" )
set( CPACK_SOURCE_PACKAGE_FILE_NAME
      "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION}${VERSION_APPEND}" )

if( WIN32 )
  set( CPACK_PACKAGE_FILE_NAME
      "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION}${VERSION_APPEND}-installer" )
endif( WIN32 )

set( CPACK_SOURCE_IGNORE_FILES "/build/;/_build/;/_darcs/;~$;${CPACK_SOURCE_IGNORE_FILES}" )

set( CPACK_COMPONENTS_ALL console_tools gui_tools divine_dev qt sys examples help)

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

  cpack_add_component( help HIDDEN DEPENDS gui_tools )

  cpack_add_component( qt HIDDEN DEPENDS gui_tools )
  cpack_add_component( sys HIDDEN )

  cpack_add_component( examples DISPLAY_NAME "Examples"
                       DESCRIPTION "Sample models" )
endif()

test_bricks( bricks )
benchmark_bricks( bricks )
