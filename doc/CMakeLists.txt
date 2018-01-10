find_program( PANDOC pandoc )
find_program( CONTEXT context )

set( MANUAL_SRC overview.md building.md using.md commands.md llvm.md sim.md divm.md dios.md )
set( MANUAL_OPT "--filter=pandoc-crossref -MlinkReferences:true -MsecPrefix:Section -N --toc" )

if ( PANDOC )
  if ( NOT CONTEXT )
    message( WARNING "context not found, skipping pdf generation" )
  endif()
  add_subdirectory( manual )
  add_subdirectory( website )
endif()

