# TODO

set( SOURCES title.mdwn overview.mdwn using.mdwn language.mdwn )

add_custom_command(
  OUTPUT manual.pdf
  DEPENDS ${SOURCES}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND markdown2pdf --toc -o ${CMAKE_CURRENT_BINARY_DIR}/manual.pdf ${SOURCES}
)

add_custom_command(
  OUTPUT manual.html
  DEPENDS ${SOURCES}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND pandoc -s --toc -o ${CMAKE_CURRENT_BINARY_DIR}/manual.html ${SOURCES}
)

add_custom_target( manual ALL
  DEPENDS manual.pdf manual.html )
