find_program( PANDOC pandoc )
find_program( PDFLATEX pdflatex )
find_program( BIBTEX bibtex )

set( MANUAL_SRC overview.mdwn building.mdwn using.mdwn llvm.mdwn )

if ( PANDOC )
  if ( NOT PDFLATEX OR NOT BIBTEX )
    message( WARNING "pdflatex or bibtex not found. Skipping PDF generation." )
  endif()
  add_subdirectory( manual )
  add_subdirectory( website )
endif()

