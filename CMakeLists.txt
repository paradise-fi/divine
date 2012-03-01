cmake_minimum_required(VERSION 2.4.7)
project( divine )

set( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake" )

if(COMMAND cmake_policy)
  cmake_policy(SET CMP0002 OLD) # FIXME
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

if(NOT CMAKE_BUILD_TYPE)
 set(CMAKE_BUILD_TYPE "Release" CACHE STRING
     "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
     FORCE)
endif(NOT CMAKE_BUILD_TYPE)

macro(appendFlag flags value)
  foreach( F ${flags} )
    set( CMAKE_${F}_FLAGS "${CMAKE_${F}_FLAGS} ${value}" )
  endforeach( F )
endmacro(appendFlag)

############################################
## USER-LEVEL OPTIONS
##

option( MPI "compile DiVinE with MPI for clustering support" ON )
option( LLVM "compile DiVinE with LLVM bitcode verification support" ON )
option( DVE "include the DVE interpreter in the build" ON )
option( COIN "include the CoIn interpreter in the build" ON )
option( MURPHI "enable Murphi model compiler" ON )
option( CURSES "link to curses library for nice statistics" ON )

option( MANUAL "build pdf/html manual" ON )
option( GUI "compile the Qt GUI" ON )
option( LTL2DSTAR "build LTL2DSTAR, for LTL support in probabilistic MC" ON )
option( HOARD "use HOARD SMP memory allocator" ON )

option( USE64 "pass -m64 to gcc to force 64b on multiarch" OFF )
option( USE32 "pass -m32 to gcc to force 32b on multiarch" OFF )

############################################
## DEVELOPER-ORIENTED OPTIONS
##

option( PERFORMANCE "compile for maximum performance (LONG compilation)" ON )
option( PROFILE "generate profiling information for lcov coverage" OFF )
option( POOLS "use pool allocation for graph vertices" ON )
option( SMALL "reduce size of the binary, disabling optional features" OFF )

###############################################
## MISC OPTION IMPLEMENTATION
##

if( NOT WIN32 )
  add_definitions( -DPOSIX )
endif( NOT WIN32 )

if( SMALL )
  add_definitions( -DO_SMALL )
endif( SMALL )

# on some mingw32, regex.h is not on the default include path
find_path( RX_PATH regex.h )
include_directories( ${RX_PATH} )
link_directories( ${RX_PATH} )

if( PERFORMANCE )
  if ( NOT SMALL )
    add_definitions( -DO_PERFORMANCE )
  else()
    message( "-- WARNING: You asked for both SMALL and PERFORMANCE. I prefer the former." )
  endif()
endif( PERFORMANCE )

if( DVE )
    include( CheckCXXSourceCompiles )
    set( CMAKE_REQUIRED_FLAGS_SAVE ${CMAKE_REQUIRED_FLAGS} )
    set( CMAKE_REQUIRED_FLAGS "-std=c++0x" )
    check_cxx_source_compiles("
        struct ConstructorTest {
            ConstructorTest(int) {}
        };

        union UnionTest {
            ConstructorTest b;
        };

        int main() { return 0; }
    " HAVE_CXX11_SUPPORT )
    set( CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS_SAVE} )

    if( HAVE_CXX11_SUPPORT )
        add_definitions( -DO_DVE )
        appendFlag( "CXX" "-std=c++0x" )
    else()
        set( DVE OFF )
        message( "-- WARNING: C++11 support not available. Disabling DVE interpreter." )
    endif()
endif( DVE )

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

if( CURSES AND NOT USE32 AND NOT USE64 )
  # the curses package we'd find probably wouldn't work anyway
  find_package( Curses )
  if( CURSES_FOUND )
    add_definitions( -DO_CURSES )
  else()
    set( CURSES OFF )
  endif()
endif()

if( COIN )
  include (CheckIncludeFileCXX)
  CHECK_INCLUDE_FILE_CXX(tr1/unordered_set HAVE_UNORDERED_SET)
  if( HAVE_UNORDERED_SET )
    add_definitions( -DO_COIN )
  else( HAVE_UNORDERED_SET )
    message("-- WARNING: unordered_set not available. Disabling CoIn interpreter.")
  endif( HAVE_UNORDERED_SET )
endif( COIN )

if( LLVM )
  include( FindLLVM )
  if( NOT LLVM_FOUND )
    message( "-- WARNING: Could not find LLVM. Disabling LLVM support." )
    set( LLVM OFF )
  else()
    if ( ${LLVM_STRING_VERSION} VERSION_LESS "3.0" )
      message( "-- WARNING: Can't use LLVM version ${LLVM_STRING_VERSION}: need 3.0" )
      set( LLVM OFF )
    else()
      add_definitions( -DO_LLVM )
      link_directories( ${LLVM_LIBRARY_DIRS} )
    endif()
  endif()
endif()

if( MANUAL )
  find_program( PANDOC pandoc )
  if ( PANDOC )
    # nothing to do
  else()
    message( "-- WARNING: Could not find pandoc, disabling pdf/html manual.")
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
    message( "-- WARNING: Could not find MPI. Disabling cluster support." )
    set( MPI OFF )
    set( MPI_INCLUDE_PATH "" )
    set( MPI_LIBRARIES "" )
    set( MPI_LIBRARY "" )
    set( MPIEXEC "" )
  endif()
endif()

if( PROFILE )
  find_program( LCOV lcov )
  find_program( LCOV_GENHTML genhtml )

  if( LCOV )
    set( LCOV_BIN "${LCOV}" )
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
    message( "-- WARNING: Could not find LCOV, disabling coverage report generation." )
    set( PROFILE OFF )
  endif()
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
    set( MURPHI OFF )
  endif()
endif()

if( GUI )
  find_package( Qt4 4.5.0 )
  if( QT4_FOUND AND QT_VERSION_MINOR GREATER 4 )
    # nothing to do
  else()
    message( "-- WARNING: Could not find Qt4 >= 4.5. Disabling GUI." )
    set( GUI OFF )
  endif()
endif( GUI )

set( DIVINE_INCLUDES
  ${divine_SOURCE_DIR}
  ${divine_BINARY_DIR}
  ${MPI_INCLUDE_PATH}
  ${LLVM_INCLUDE_DIRS}
  ${divine_SOURCE_DIR}/divine/legacy )

##########################################
## SOURCE DIRECTORIES
##

add_custom_target( check )
add_custom_target( unit )

add_subdirectory( wibble )
add_subdirectory( lpsolve )

if( HOARD )
  add_subdirectory( hoard )
endif( HOARD )

if( LTL2DSTAR )
  add_subdirectory( ltl2ba )
  add_subdirectory( ltl2dstar )
endif()

if( MURPHI )
  add_subdirectory( murphi )
endif()

add_subdirectory( divine )
add_subdirectory( tools )
add_subdirectory( examples )

if( GUI )
  add_subdirectory( gui )
endif( GUI )

if( MANUAL )
  add_subdirectory( manual )
  add_subdirectory( website )
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
## FIXME: better dependency detection

install(FILES README RENAME README.txt DESTINATION ./ COMPONENT sys)
install(FILES COPYING RENAME COPYING.txt DESTINATION ./ COMPONENT sys)
install(FILES AUTHORS RENAME AUTHORS.txt DESTINATION ./ COMPONENT sys)
install(FILES NEWS RENAME NEWS.txt DESTINATION ./ COMPONENT sys)

if( WIN32 )
  find_file( REGEX_LIB libgnurx-0.dll )
  find_file( GCC_SJL_LIB libgcc_s_sjlj-1.dll )

  install(FILES ${REGEX_LIB} DESTINATION ./ COMPONENT sys)
  install(FILES ${GCC_SJL_LIB} DESTINATION ./ COMPONENT qt)
endif( WIN32 )

########################################
## BINARY INSTALLER
##

set( CPACK_PACKAGE_DESCRIPTION_SUMMARY "DiViNE Distributed and Parallel Verification Environment" )
set( CPACK_PACKAGE_VENDOR "ParaDiSe Laboratory" )
set( CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README" )
set( CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING" )
set( CPACK_PACKAGE_VERSION_MAJOR "2" )
set( CPACK_PACKAGE_VERSION_MINOR "5" )
set( CPACK_PACKAGE_VERSION_PATCH "1" )
set( CPACK_PACKAGE_INSTALL_DIRECTORY "DiVinE-2.5" )

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
