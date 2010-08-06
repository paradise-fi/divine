find_program( GHC ghc )

set( GHCFLAGS -O2 -threaded )

macro( haskell_so name )
add_custom_target(
  ${name}
  DEPENDS divine
  COMMAND ${GHC} ${GHCFLAGS} --make -no-hs-main -optl '-shared' -odir "." -hidir "." -i${CMAKE_CURRENT_SOURCE_DIR} -o ${name}.dummy.so ${CMAKE_CURRENT_SOURCE_DIR}/${name}.hs
  COMMAND ${GHC} ${GHCFLAGS} --make -no-hs-main -optl '-shared' -odir "." -hidir "." -i${CMAKE_CURRENT_SOURCE_DIR} -o ${name}.so ${CMAKE_CURRENT_SOURCE_DIR}/${name}.hs ${CMAKE_CURRENT_SOURCE_DIR}/Divine/module_init.c -optc "-DMODULE=${name}"
)
endmacro( haskell_so )

add_custom_target( examples )

if( NOT WIN32 ) # currently fails to build on windows
add_library( BenchmarkC MODULE BenchmarkC.c )
set_target_properties( BenchmarkC PROPERTIES EXCLUDE_FROM_ALL ON PREFIX "" )
add_dependencies( examples BenchmarkC )
endif()

if( GHC )
  if( NOT USE64 AND NOT USE32 AND NOT WIN32 )
     set( HS_EXAMPLES ON )
  else()
      if( WIN32 )
        message( "-- WARNING: We don't know how to build haskell shared libs on Windows." )
      else()
        message( "-- WARNING: Multiarch is not supported by GHC. Not compiling haskell example models." )
      endif()
  endif()
else( GHC )
  message( "-- WARNING: Could not find GHC. Not compiling haskell example models." )
  message( "   (to tell me where GHC lives, use -DGHC=/path/to/ghc, thanks!)" )
endif( GHC )

if( HS_EXAMPLES )
haskell_so( BenchmarkHs )
add_dependencies( examples BenchmarkHs )
endif( HS_EXAMPLES )

if( WIN32 )
  file( GLOB DVE_SAMPLE *.dve )

  install( FILES ${DVE_SAMPLE} DESTINATION examples COMPONENT examples )
endif( WIN32 )
