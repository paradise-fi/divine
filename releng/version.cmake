find_program( SHA1SUM NAMES sha1sum sha1 )

if( NOT SHA1SUM )
  message( "Did not find sha1sum, version will be imprecise." )
endif( NOT SHA1SUM )

file( READ releng/version DIVINE_MAJOR )
file( READ releng/patchlevel PATCHLEVEL )
string( STRIP "${DIVINE_MAJOR}" DIVINE_MAJOR )
string( STRIP "${PATCHLEVEL}" PATCHLEVEL )
set( DIVINE_VERSION "${DIVINE_MAJOR}.${PATCHLEVEL}" )

macro(versionExtract out n in)
   string(REGEX REPLACE "([0-9]*)\\.([0-9]*)\\.?([0-9]*)?" "\\${n}" ${out} ${in})
endmacro()

versionExtract( CPACK_PACKAGE_VERSION_MAJOR 1 ${DIVINE_VERSION} )
versionExtract( CPACK_PACKAGE_VERSION_MINOR 2 ${DIVINE_VERSION} )
versionExtract( CPACK_PACKAGE_VERSION_PATCH 3 ${DIVINE_VERSION} )

