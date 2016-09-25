set( SRCDIR ${CMAKE_CURRENT_SOURCE_DIR} )

macro( page to srcdir opts )
  set( sources "" )
  foreach( source ${ARGN} )
      string( REPLACE "../" "_" mangled ${source} )
      list( APPEND sources "${CMAKE_CURRENT_BINARY_DIR}/subst.${mangled}" )
      add_custom_command(
        VERBATIM
        OUTPUT subst.${mangled}
        DEPENDS ${srcdir}/${source}
        WORKING_DIRECTORY ${SRCDIR}
        COMMAND sed -e s,@version@,${DIVINE_VERSION},g
                    -e "s,DIVINE,<span class='divine'>DIVINE</span>," ${srcdir}/${source} >
                ${CMAKE_CURRENT_BINARY_DIR}/subst.${mangled}
      )
  endforeach()
  add_custom_command(
    OUTPUT ${to}
    DEPENDS ${sources} template.html
    COMMAND pandoc --to=html -s --smart
            -o ${to}
            --template ${SRCDIR}/template.html
            -V version:${DIVINE_VERSION}
            ${opts}
            ${sources}
  )
  list( APPEND pages ${to} )
endmacro()

macro( pages sourcedir )
  foreach( source ${ARGN} )
    string( REPLACE ".md" ".html" target "${source}" )
    page( ${target} ${sourcedir} "" ${source} )
  endforeach()
endmacro()

macro( resources )
  foreach( source ${ARGN} )
    string( REPLACE "../" "" target "${source}" )
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${target}
      DEPENDS ${SRCDIR}/${source}
      COMMAND cmake -E copy ${SRCDIR}/${source} .
    )
    list( APPEND resources ${target} )
  endforeach()
endmacro()

add_custom_command(
 OUTPUT papers.html
 DEPENDS ${SRCDIR}/../bib2md.pl template.html ${SRCDIR}/../papers.bib
 COMMAND perl ${SRCDIR}/../bib2md.pl ${SRCDIR}/../papers.bib |
         pandoc -s --smart --template ${SRCDIR}/template.html > papers.html )

page( whatsnew.html .. "--toc" NEWS )
page( manual.html ../manual "--toc;-N" ${MANUAL_SRC} )
pages( . download.md index.md status.md )
resources( style.css um-400.ttf um-700.ttf lato-400.ttf lato-700.ttf ../papers.bib )
add_custom_target( website ALL DEPENDS ${pages} ${resources} papers.html )
