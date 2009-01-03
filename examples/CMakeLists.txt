set( HC "ghc" )
set( HCFLAGS -O2 -threaded )
file( GLOB hssrc "Divine/*.hs" )

macro( haskell_so name )
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${name}.hs
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${name}.in.hs
  COMMAND DrIFT ${CMAKE_CURRENT_SOURCE_DIR}/${name}.in.hs > ${name}.hs
)

add_custom_target(
  ${name}
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${name}.hs
  COMMAND ${HC} ${HCFLAGS} --make -no-hs-main -optl '-shared' -optl "-L${divine_BUILD_DIR}" -optl "-ldivine" -optc "-I." -optc "-I${CMAKE_SOURCE_DIR}" -optc "-DMODULE=${name}" -odir "." -hidir "." -o ${name}.so ${hssrc} ${name}.hs ${CMAKE_CURRENT_SOURCE_DIR}/Divine/module_init.c
)
endmacro( haskell_so )

haskell_so( BenchmarkHs )
add_library( BenchmarkC MODULE BenchmarkC.c )
set_target_properties( BenchmarkC PROPERTIES EXCLUDE_FROM_ALL ON PREFIX "" )

set_source_files_properties( ghc-make PROPERTIES SYMBOLIC ON )
add_custom_target( examples )
add_dependencies( examples BenchmarkHs BenchmarkC )
