include_directories( ${DIVINE_INCLUDES} )

link_libraries( libdivine wibble )
if( MURPHI )
    link_libraries( murphi )
endif()

if( NOT WIN32 )
  link_libraries( libdivine pthread )
endif( NOT WIN32 )

if( HOARD )
  link_libraries( hoard )
endif( HOARD )

set( PACKJARS "${CMAKE_CURRENT_SOURCE_DIR}/packjars.pl" )
set( MAKE_STRING_CONST "${CMAKE_CURRENT_SOURCE_DIR}/make-string-const.pl" )

set( COMBINE_M4_H "${CMAKE_CURRENT_BINARY_DIR}/combine.m4.h" )
set( COMBINE_M4 "${CMAKE_CURRENT_SOURCE_DIR}/combine.m4" )

set( JARS "${CMAKE_SOURCE_DIR}/nips-compiler-jars/pml2s.jar"
          "${CMAKE_SOURCE_DIR}/nips-compiler-jars/jdom.jar" )

set( PML_C "${CMAKE_CURRENT_BINARY_DIR}/divine.compile-pml" )
set( NIPS_ASM "${CMAKE_CURRENT_SOURCE_DIR}/compile-pml.pl" )

add_executable( divine divine.cpp dvecompile.cpp compile.cpp combine.h ${COMBINE_M4_H} )
add_executable( divine.simulate simulator.cpp )
add_executable( divine.simple simple.cpp )

if( WIN32 )
  set( INSTALL_BIN_DEST ./ )
else( WIN32 )
  set( INSTALL_BIN_DEST bin )
endif( WIN32 )

install( TARGETS divine
                 divine.simulate
         DESTINATION ${INSTALL_BIN_DEST} COMPONENT console_tools )

if( NOT WIN32 )
  # This code uses raw pthreads, so no-go on win32.
  add_executable( divine.probabilistic probabilistic.cpp )
  target_link_libraries( divine.probabilistic lpsolve )
  install( TARGETS divine.probabilistic DESTINATION ${INSTALL_BIN_DEST} COMPONENT console_tools )
endif( NOT WIN32 )

install( PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/divine.compile-pml DESTINATION ${INSTALL_BIN_DEST} COMPONENT console_tools )

set( PROMELA_JARS "BUNDLE" CACHE STRING "Either path to the ProMeLa/NIPS JAR files or BUNDLE" )

if ( PROMELA_JARS STREQUAL "BUNDLE" )
  set( JAR_DEPS ${JARS} )
else()
  set( JAR_DEPS "" )
endif()

add_custom_command(
  OUTPUT ${PML_C}
  DEPENDS packjars.pl ${JAR_DEPS} ${NIPS_ASM} ${PACKJARS}
  COMMAND ${PERL_EXECUTABLE} ${PACKJARS} "${PROMELA_JARS}" ${NIPS_ASM} ${JARS} > ${PML_C}
  COMMAND chmod +x ${PML_C} )

add_custom_command(
  OUTPUT ${COMBINE_M4_H}
  DEPENDS ${COMBINE_M4} ${MAKE_STRING_CONST}
  COMMAND ${PERL_EXECUTABLE} ${MAKE_STRING_CONST} "const char *combine_m4 = " ${COMBINE_M4} > ${COMBINE_M4_H} )

add_custom_target(
  make-divine.compile-pml ALL
  DEPENDS ${PML_C} )
