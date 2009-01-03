find_program( GHC ghc )
find_program( DRIFT DrIFT )

set( GHCFLAGS -O2 -threaded )

macro( haskell_so name )
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${name}.hs
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${name}.in.hs
  COMMAND DrIFT ${CMAKE_CURRENT_SOURCE_DIR}/${name}.in.hs > ${name}.hs
)

add_custom_target(
  ${name}
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${name}.hs
  DEPENDS divine
  COMMAND ${GHC} ${GHCFLAGS} --make -no-hs-main -optl '-shared' -optl "-L${divine_BUILD_DIR}" -optl "-ldivine" -optc "-I." -optc "-I${CMAKE_SOURCE_DIR}" -optc "-DMODULE=${name}" -odir "." -hidir "." -i${CMAKE_CURRENT_SOURCE_DIR} -o ${name}.so ${name}.hs ${CMAKE_CURRENT_SOURCE_DIR}/Divine/module_init.c
)
endmacro( haskell_so )

add_library( BenchmarkC MODULE BenchmarkC.c )
set_target_properties( BenchmarkC PROPERTIES EXCLUDE_FROM_ALL ON PREFIX "" )

add_custom_target( examples )
add_dependencies( examples BenchmarkC )

if( GHC )
  if( DRIFT )
    set( HS_EXAMPLES ON )
  else( DRIFT )
    message( "-- WARNING: Could not find DrIFT. Not compiling haskell example models." )
    message( "   (to tell me where DrIFT lives, use -DDRIFT=/path/to/DrIFT, thanks!)" )
  endif( DRIFT )
else( GHC )
  message( "-- WARNING: Could not find GHC. Not compiling haskell example models." )
  message( "   (to tell me where GHC lives, use -DGHC=/path/to/ghc, thanks!)" )
endif( GHC )

if( HS_EXAMPLES )
haskell_so( BenchmarkHs )
add_dependencies( examples BenchmarkHs )
endif( HS_EXAMPLES )
