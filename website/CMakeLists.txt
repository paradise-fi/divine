macro( page to srcdir opts )
  set( sources "" )
  foreach( source ${ARGN} )
    list( APPEND sources "${srcdir}/${source}" )
  endforeach()
  add_custom_command(
    OUTPUT ${to}
    DEPENDS ${sources} template.html
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND pandoc --to=html -s --smart
            -o ${CMAKE_CURRENT_BINARY_DIR}/${to}
            --template template.html
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
page( manual.html ../manual "--toc;-N"
        title.mdwn overview.mdwn building.mdwn commands.mdwn
        using.mdwn llvm.mdwn dve.mdwn timed.mdwn cesmi.mdwn
        dess.mdwn ../HACKING )
pages( . download.mdwn index.mdwn status.mdwn )
resources( style.css hydra-api.js button.png button-over.png divine-logo.png trac-logo.png )
add_custom_target( website ALL DEPENDS ${pages} ${resources} )
