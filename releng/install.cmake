include( GNUInstallDirs )
install( FILES doc/README doc/COPYING doc/AUTHORS doc/NEWS
         DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT doc )

set( CPACK_PACKAGE_VENDOR "ParaDiSe Laboratory" )
set( CPACK_PACKAGE_DESCRIPTION_FILE "${divine_SOURCE_DIR}/doc/README" )
set( CPACK_RESOURCE_FILE_LICENSE "${divine_SOURCE_DIR}/doc/COPYING" )
set( CPACK_PACKAGE_VERSION ${DIVINE_VERSION} )

set( CPACK_SOURCE_PACKAGE_FILE_NAME
     "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION}${VERSION_APPEND}" )

set( CPACK_SOURCE_IGNORE_FILES "^local.make$;/build/;/_build/;/_darcs/;~$;${CPACK_SOURCE_IGNORE_FILES}" )

include( CPack )
