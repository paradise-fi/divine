# TODO

set( SOURCES title.mdwn overview.mdwn commands.mdwn using.mdwn language.mdwn cesmi.mdwn )

if( PDFLATEX AND BIBTEX )
  add_custom_command(
    OUTPUT manual.pdf
    DEPENDS ${SOURCES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND pandoc --smart --toc -o ${CMAKE_CURRENT_BINARY_DIR}/manual.pdf ${SOURCES}
  )
  set( PDFMAN "manual.pdf" )
endif()

add_custom_command(
  OUTPUT manual.html
  DEPENDS ${SOURCES}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND pandoc --smart -s --toc -o ${CMAKE_CURRENT_BINARY_DIR}/manual.html ${SOURCES}
)

add_custom_command(
  OUTPUT divine.1
  DEPENDS commands.mdwn
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND pandoc --smart -s --to man -o ${CMAKE_CURRENT_BINARY_DIR}/divine.1 commands.mdwn
)

add_custom_target( manual ALL DEPENDS ${PDFMAN} manual.html divine.1 )

include( GNUInstallDirs )

if( WIN32 )
  install( FILES manual.html DESTINATION ./ COMPONENT sys )
else()
  install( FILES divine.1 DESTINATION ${CMAKE_INSTALL_FULL_MANDIR}/man1 COMPONENT doc )
  install( FILES manual.html ${PDFMAN} DESTINATION ${CMAKE_INSTALL_FULL_DOCDIR} COMPONENT doc )
endif()
