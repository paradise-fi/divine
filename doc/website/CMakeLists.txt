set( SRCDIR ${CMAKE_CURRENT_SOURCE_DIR} )

macro( page to srcdir opts )
  set( sources "" )
  set( out "${CMAKE_CURRENT_BINARY_DIR}/${to}" )
  list( APPEND pages ${out} )

  foreach( source ${ARGN} )
    list( APPEND sources "${srcdir}/${source}" )
  endforeach()

  add_custom_command(
    OUTPUT ${out}
    DEPENDS ${sources} template.html ${SRCDIR}/../panmd.pl
    WORKING_DIRECTORY ${SRCDIR}
    COMMAND perl ${SRCDIR}/../panmd.pl --to=html ${opts} ${PANDOC_SMART_OPT}
            version=${DIVINE_VERSION} major=${DIVINE_MAJOR} ${sources}
            | sed -e "s,:DIVINE:,<span class=\"divine\">DIVINE</span>,g" > ${out}
    VERBATIM
  )
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
         ${PANDOC} -s ${PANDOC_SMART_OPT} --template ${SRCDIR}/template.html > papers.html )

set( INSTALL_SH ${CMAKE_SOURCE_DIR}/releng/install.sh )
set( INSTALL_VM_SH ${CMAKE_SOURCE_DIR}/releng/install-vm.sh )

add_custom_command(
  OUTPUT install.sh
  DEPENDS ${INSTALL_SH}
  COMMAND sed "s/@version@/${DIVINE_VERSION}/" ${INSTALL_SH} > install.sh )

add_custom_command(
  OUTPUT install-vm.sh
  COMMAND ${CMAKE_COMMAND} -E copy ${INSTALL_VM_SH} install-vm.sh
  VERBATIM )

page( whatsnew.html .. "--toc --toc-depth 1" NEWS )
page( roadmap.html .. "" ROADMAP )
page( manual.html ../manual "${MANUAL_OPT}" ${MANUAL_SRC} )
pages( . download.md index.md status.md previously.md )
resources( style.css um-400.ttf um-700.ttf lato-400.ttf lato-700.ttf ../papers.bib )
add_custom_target( website ALL DEPENDS ${pages} ${resources} papers.html install.sh install-vm.sh )
