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

option( PROFILE "generate profiling information for lcov coverage" OFF )
option( HOARD "use HOARD SMP memory allocator" ON )
option( POOLS "use pool allocation for graph vertices" ON )
option( USE64 "pass -m64 to gcc to force 64b on multiarch" OFF )
option( USE32 "pass -m32 to gcc to force 32b on multiarch" OFF )
option( MPI "compile DiVinE with MPI for clustering support" ON )
option( GUI "compile the Qt GUI" ON )
option( MURPHI "enable Murphi model compiler" ON )
option( PERFORMANCE "compile for maximum performance (LONG compilation)" ON )

if( NOT WIN32 )
  add_definitions( -DPOSIX )
endif( NOT WIN32 )

if( PERFORMANCE )
  add_definitions( -DPERFORMANCE )
endif( PERFORMANCE )

if( PROFILE )
  appendFlag( "CXX;C" "-fprofile-arcs -ftest-coverage" )
  link_libraries( "gcov" )
endif( PROFILE )

if( USE64 )
  appendFlag( "CXX;C;EXE_LINKER;SHARED_LINKER" "-m64" )
  add_definitions( -DUSE_GCC_M64 )
endif( USE64 )

if( USE32 )
  appendFlag( "CXX;C;EXE_LINKER;SHARED_LINKER" "-m32" )
  add_definitions( -DUSE_GCC_M32 )
endif( USE32 )

if( NOT POOLS )
  add_definitions( "-DDISABLE_POOLS" )
endif( NOT POOLS )

if( GUI )
  find_package( Qt4 4.5.0 )
endif( GUI )

if( NOT USE32 AND NOT USE64 )
  # the curses package we'd find probably wouldn't work anyway
  find_package( Curses )
  if( CURSES_FOUND )
    add_definitions( -DHAVE_CURSES )
  endif()
endif()

# on some mingw32, regex.h is not on the default include path
find_path( RX_PATH regex.h )
include_directories( ${RX_PATH} )
link_directories( ${RX_PATH} )

if( MPI AND NOT WIN32 )
include( FindMPI )
endif( MPI AND NOT WIN32 )

if( MPI AND MPI_FOUND )
  add_definitions( "-DHAVE_MPI" )
else( MPI AND MPI_FOUND )
  set( MPI_INCLUDE_PATH "" )
  set( MPI_LIBRARIES "" )
  set( MPI_LIBRARY "" )
  if( MPI )
    message( "-- WARNING: Could not find MPI. Disabling cluster support." )
  endif( MPI )
  set( MPI OFF )
  set( MPIEXEC "" )
endif( MPI AND MPI_FOUND )

set( DIVINE_INCLUDES
  ${divine_SOURCE_DIR}
  ${divine_BINARY_DIR}
  ${MPI_INCLUDE_PATH}
  ${divine_SOURCE_DIR}/divine/legacy )

find_program( SHA1SUM sha1sum )

if( NOT SHA1SUM )
  message( "I could use a working sha1sum program for the build. Please install one or supply me with a path if you already have it. Thank you." )
endif( NOT SHA1SUM )

add_custom_target( check )
add_custom_target( unit )
add_custom_target( lcovd-unit )

if( PROFILE )
  find_program( LCOV lcov )
  find_program( LCOV_GENHTML genhtml )
  if( NOT LCOV )
    message( "-- WARNING: Could not find LCOV, disabling coverage report generation." )
  endif( NOT LCOV )
endif( PROFILE )

if( LCOV )
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
endif( LCOV )

add_subdirectory( wibble )
if( HOARD )
  add_subdirectory( hoard )
endif( HOARD )
add_subdirectory( lpsolve )

find_program( BYACC "byacc" )
find_package( FLEX )
find_package( BYACC )

if( MURPHI AND FLEX_FOUND AND BYACC_FOUND AND BYACC )
add_subdirectory( murphi )
add_definitions( -DHAVE_MURPHI )
else()
set( MURPHI OFF )
endif()

add_subdirectory( divine )
add_subdirectory( tools )
add_subdirectory( examples )

if( GUI )
  if( QT4_FOUND AND QT_VERSION_MINOR GREATER 4 )
    add_subdirectory( gui )
  else()
    message( "-- WARNING: Could not find Qt4 >= 4.5. Disabling GUI." )
  endif()
endif( GUI )

add_dependencies( lcovd-unit unit )

if( LCOV )
  lcov_collect( "1" )
  add_dependencies( lcovd-unit lcov-collect-1 )
  add_dependencies( lcov-collect-1 unit )
endif( LCOV )

add_dependencies( check lcovd-unit )
add_dependencies( check functional )

add_subdirectory( test )

# FIXME: better dependency detection
if( WIN32 )
  find_file( REGEX_LIB libgnurx-0.dll )
  find_file( GCC_SJL_LIB libgcc_s_sjlj-1.dll )

  install(FILES ${REGEX_LIB} DESTINATION ./ COMPONENT sys)
  install(FILES ${GCC_SJL_LIB} DESTINATION ./ COMPONENT qt)
endif( WIN32 )

install(FILES README RENAME README.txt DESTINATION ./ COMPONENT sys)
install(FILES COPYING RENAME COPYING.txt DESTINATION ./ COMPONENT sys)
install(FILES AUTHORS RENAME AUTHORS.txt DESTINATION ./ COMPONENT sys)
install(FILES WHATS_NEW RENAME WHATS_NEW.txt DESTINATION ./ COMPONENT sys)        

set( CPACK_PACKAGE_DESCRIPTION_SUMMARY "DiViNE Distributed and Parallel Verification Environment" )
set( CPACK_PACKAGE_VENDOR "ParaDiSe Laboratory" )
set( CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README" )
set( CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING" )
set( CPACK_PACKAGE_VERSION_MAJOR "2" )
set( CPACK_PACKAGE_VERSION_MINOR "1" )
set( CPACK_PACKAGE_VERSION_PATCH "0" )
set( CPACK_PACKAGE_INSTALL_DIRECTORY "divine-2.1" )

set( CPACK_COMPONENTS_ALL console_tools gui_tools divine_dev wibble_dev qt sys examples help)

if(WIN32)
  set(CPACK_NSIS_MENU_LINKS "./divine.ide.exe" "DiViNE IDE"
                            "./README.txt" "README"
                            "./COPYING.txt" "COPYING"
                            "./AUTHORS.txt" "AUTHORS"
                            "./WHATS_NEW.txt" "WHATS_NEW")
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
