find_program( PANDOC pandoc )
find_program( PDFLATEX pdflatex )
find_program( BIBTEX bibtex )

set( MANUAL_SRC overview.md building.md using.md commands.md llvm.md sim.md divm.md dios.md )
set( MANUAL_OPT "-F pandoc-crossref -MlinkReferences:true -MsecPrefix:Section" )

if ( PANDOC )
  if ( NOT PDFLATEX OR NOT BIBTEX )
    message( WARNING "pdflatex or bibtex not found. Skipping PDF generation." )
  endif()
  add_subdirectory( manual )
  add_subdirectory( website )
endif()

