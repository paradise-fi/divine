# TODO

set( SOURCES title.mdwn overview.mdwn using.mdwn language.mdwn cesmi.mdwn )

if( PDFLATEX AND BIBTEX )
  add_custom_command(
    OUTPUT manual.pdf
    DEPENDS ${SOURCES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND pandoc --toc -o ${CMAKE_CURRENT_BINARY_DIR}/manual.pdf ${SOURCES}
  )
  set( PDFMAN "manual.pdf" )
endif()

add_custom_command(
  OUTPUT manual.html
  DEPENDS ${SOURCES}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND pandoc -s --toc -o ${CMAKE_CURRENT_BINARY_DIR}/manual.html ${SOURCES}
)

add_custom_command(
  OUTPUT divine.1
  DEPENDS commands.mdwn
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND pandoc -s --to man -o ${CMAKE_CURRENT_BINARY_DIR}/divine.1 commands.mdwn
)

add_custom_target( manual ALL DEPENDS ${PDFMAN} manual.html divine.1 )
