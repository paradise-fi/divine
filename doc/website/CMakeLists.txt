macro( page to srcdir opts )
  set( sources "" )
  foreach( source ${ARGN} )
      string( REPLACE "../" "_" mangled ${source} )
      list( APPEND sources "${CMAKE_CURRENT_BINARY_DIR}/subst.${mangled}" )
      add_custom_command(
        OUTPUT subst.${mangled}
        DEPENDS ${srcdir}/${source}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND sed -e s,@version@,${DIVINE_VERSION},g ${srcdir}/${source} >
                ${CMAKE_CURRENT_BINARY_DIR}/subst.${mangled}
      )
  endforeach()
  add_custom_command(
    OUTPUT ${to}
    DEPENDS ${sources} template.html
    COMMAND pandoc --to=html -s --smart
            -o ${to}
            --template ${CMAKE_CURRENT_SOURCE_DIR}/template.html
            -V version:${DIVINE_VERSION}
            ${opts}
            ${sources}
  )
  list( APPEND pages ${to} )
endmacro()

macro( pages sourcedir )
  foreach( source ${ARGN} )
    string( REPLACE ".mdwn" ".html" target "${source}" )
    page( ${target} ${sourcedir} "" ${source} )
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

page( whatsnew.html .. "--toc" NEWS )
page( manual.html ../manual "--toc;-N" ${MANUAL_SRC} )
pages( . download.mdwn index.mdwn status.mdwn )
resources( style.css hydra-api.js button.png button-over.png divine-logo.png trac-logo.png )
add_custom_target( website ALL DEPENDS ${pages} ${resources} )
