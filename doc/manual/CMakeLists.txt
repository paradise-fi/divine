set( SOURCES title.md ${MANUAL_SRC} )
set( SRCDIR ${CMAKE_CURRENT_SOURCE_DIR} )
set( BINDIR ${CMAKE_CURRENT_BINARY_DIR} )

if( CONTEXT )
  add_custom_command(
    OUTPUT manual.pdf
    DEPENDS ${SOURCES} ${SRCDIR}/../panmd.pl
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND perl ${SRCDIR}/../panmd.pl version=${DIVINE_VERSION} ${MANUAL_OPT} ${PANDOC_SMART_OPT}
            --to=context --output=${BINDIR}/manual.pdf ${SOURCES}
    VERBATIM
  )
  set( PDFMAN "manual.pdf" )
endif()

if( GUI AND QCOLLECTIONGENERATOR_EXECUTABLE )
    add_custom_command(OUTPUT divine.qhc manual.qch
                       COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/divine.qhcp .
                       COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/manual.qhp .
                       COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/about.txt .
                       COMMAND ${QT_QCOLLECTIONGENERATOR_EXECUTABLE} divine.qhcp
                       DEPENDS manual.html ${CMAKE_CURRENT_SOURCE_DIR}/divine.qhcp
                               ${CMAKE_CURRENT_SOURCE_DIR}/manual.qhp )
  set( QTHELP "divine.qhc;manual.qch" )
endif()

separate_arguments( HTML_OPT UNIX_COMMAND ${MANUAL_OPT} )

add_custom_command(
  OUTPUT manual.html
  DEPENDS ${SOURCES}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${PANDOC} ${PANDOC_SMART_OPT} -s ${HTML_OPT} -N --toc -o ${CMAKE_CURRENT_BINARY_DIR}/manual.html ${SOURCES}
  VERBATIM
)

add_custom_command(
  OUTPUT divine.1
  DEPENDS commands.md
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND sed -e "/^# /d" -e "s,^##,#," < commands.md |
          ${PANDOC} ${PANDOC_SMART_OPT} -s --to man -o ${CMAKE_CURRENT_BINARY_DIR}/divine.1
  VERBATIM
)

add_custom_target( manual ALL DEPENDS ${PDFMAN} ${QTHELP} manual.html divine.1 )

include( GNUInstallDirs )

if( WIN32 )
  install( FILES manual.html DESTINATION ./ COMPONENT sys )
else()
  install( FILES "${CMAKE_CURRENT_BINARY_DIR}/divine.1"
           DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 COMPONENT doc )
  install( FILES "${CMAKE_CURRENT_BINARY_DIR}/manual.html"
           DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT doc )
  if ( PDFMAN )
      install( FILES "${CMAKE_CURRENT_BINARY_DIR}/manual.pdf"
               DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT doc )
  endif()
endif()
