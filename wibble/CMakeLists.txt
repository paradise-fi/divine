PROJECT( wibble )

if ( HAVE_GC )
  set ( LINK_WITH_LIBGC "-lgc -lgccpp" )
endif ( HAVE_GC )

file( GLOB WSRC_TOP *.cpp )
file( GLOB WSRC_CMDL commandline/*.cpp )
file( GLOB H_TOP *.h )
file( GLOB H_CMDL commandline/*.h )
file( GLOB TCC_TOP *.tcc )

SET( WSRC ${WSRC_TOP} ${WSRC_CMDL} )

set_source_files_properties( ${WSRC} COMPILE_FLAGS -fPIC )

INCLUDE_DIRECTORIES( ${wibble_SOURCE_DIR}/.. ${wibble_BINARY_DIR}/.. ${BOOST_INCLUDE_PATH} )
INSTALL_FILES( /include/wibble .h ${H_TOP} )
INSTALL_FILES( /include/wibble .h shared.h )
INSTALL_FILES( /include/wibble .tcc ${TCC_TOP} )
INSTALL_FILES( /include/wibble/commandline .h ${H_CMDL} )
INSTALL_FILES( /include/wibble .h ${wibble_BINARY_DIR}/config.h )

CONFIGURE_FILE( ${wibble_SOURCE_DIR}/config.h.cmake-in
                ${wibble_BINARY_DIR}/config.h )
CONFIGURE_FILE( ${wibble_SOURCE_DIR}/shared.h.cmake-in
                ${wibble_BINARY_DIR}/shared.h )

add_definitions( ${OPT_FLAGS} ${GC_DEFS} )
ADD_LIBRARY( wibble STATIC ${WSRC} )
SET_TARGET_PROPERTIES( wibble PROPERTIES DEFINE_SYMBOL WIBBLE_BUILD_FINAL )
TARGET_LINK_LIBRARIES( wibble ${GC_LIBS} )
INSTALL_TARGETS( /lib wibble )

ADD_SUBDIRECTORY( tests )

CONFIGURE_FILE( ${wibble_SOURCE_DIR}/libwibble.pc.cmake-in ${wibble_BINARY_DIR}/libwibble.pc @ONLY )
INSTALL_FILES( /lib/pkgconfig FILES libwibble.pc )
install_files( /share/aclocal FILES libwibble.m4 )

IF( XXX_BUILD_TESTING )
  SET( TEST_DIR ${wibble_BINARY_DIR}/tests/work/ )
  ADD_LIBRARY( wibble-testing SHARED ${WSRC} )
  SET_TARGET_PROPERTIES( wibble-testing
                         PROPERTIES DEFINE_SYMBOL WIBBLE_BUILD_TESTING )
  TARGET_LINK_LIBRARIES( wibble-testing ${GC_LIBS} )
ENDIF( XXX_BUILD_TESTING )
