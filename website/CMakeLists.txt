macro( page from to )
  add_custom_command(
    OUTPUT ${to}
    DEPENDS ${from} template.html
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND pandoc --to=html -s
            -o ${CMAKE_CURRENT_BINARY_DIR}/${to}
            --template template.html
            ${from}
  )
  list( APPEND pages ${to} )
endmacro()

macro( pages sourcedir )
  foreach( source ${ARGN} )
    string( REPLACE ".mdwn" ".html" target "${source}" )
    page( ${sourcedir}/${source} ${target} )
  endforeach()
endmacro()

macro( resources )
  foreach( source ${ARGN} )
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${source}
      DEPENDS ${source}
      COMMAND cmake -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${source} .
    )
    list( APPEND resources ${source} )
  endforeach()
endmacro()

page( ../HACKING development.html )
page( ../NEWS whatsnew.html )
pages( ../manual title.mdwn overview.mdwn using.mdwn language.mdwn )
pages( . contact.mdwn download.mdwn index.mdwn )
resources( style.css button.png button-over.png divine-logo.png trac-logo.png )
add_custom_target( website ALL DEPENDS ${pages} ${resources} )
