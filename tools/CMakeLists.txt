include_directories( ${DIVINE_INCLUDES} )
link_libraries( libdivine wibble pthread )
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

add_executable( divine divine.cpp )
add_executable( divine.simulate simulator.cpp )
add_executable( divine.combine combine.cpp ${COMBINE_M4_H} )

add_executable( divine.probabilistic probabilistic.cpp )
target_link_libraries( divine.probabilistic lpsolve )

install( TARGETS divine divine.simulate DESTINATION bin )

add_custom_command(
  OUTPUT ${PML_C}
  DEPENDS packjars.pl ${JARS} ${NIPS_ASM} ${PACKJARS}
  COMMAND ${PERL_EXECUTABLE} ${PACKJARS} ${NIPS_ASM} ${JARS} > ${PML_C}
  COMMAND chmod +x ${PML_C} )

add_custom_command(
  OUTPUT ${COMBINE_M4_H}
  DEPENDS ${COMBINE_M4} ${MAKE_STRING_CONST}
  COMMAND ${PERL_EXECUTABLE} ${MAKE_STRING_CONST} "const char *combine_m4 = " ${COMBINE_M4} > ${COMBINE_M4_H} )

add_custom_target(
  make-divine.compile-pml ALL
  DEPENDS ${PML_C} )
