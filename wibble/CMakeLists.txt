PROJECT( wibble )

file( GLOB WSRC_TOP *.cpp )
file( GLOB WSRC_CMDL commandline/*.cpp )
file( GLOB WSRC_SYS sys/*.cpp )
file( GLOB H_TOP *.h )
file( GLOB H_CMDL commandline/*.h )
file( GLOB H_SYS sys/*.h )
file( GLOB TCC_TOP *.tcc )

SET( WSRC ${WSRC_TOP} ${WSRC_CMDL} ${WSRC_SYS} )

set_source_files_properties( ${WSRC} COMPILE_FLAGS -fPIC )

INCLUDE_DIRECTORIES( ${wibble_SOURCE_DIR}/.. ${wibble_BINARY_DIR}/.. ${BOOST_INCLUDE_PATH} )
INSTALL_FILES( /include/wibble .h ${H_TOP} )
INSTALL_FILES( /include/wibble .tcc ${TCC_TOP} )
INSTALL_FILES( /include/wibble/commandline .h ${H_CMDL} )
INSTALL_FILES( /include/wibble/sys .h ${H_CMDL} )
INSTALL_FILES( /include/wibble .h ${wibble_BINARY_DIR}/config.h )

CONFIGURE_FILE( ${wibble_SOURCE_DIR}/config.h.cmake-in
                ${wibble_BINARY_DIR}/config.h )

add_definitions( ${OPT_FLAGS} )
ADD_LIBRARY( wibble STATIC ${WSRC} )
SET_TARGET_PROPERTIES( wibble PROPERTIES DEFINE_SYMBOL WIBBLE_BUILD_FINAL )
TARGET_LINK_LIBRARIES( wibble )
INSTALL_TARGETS( /lib wibble )

ADD_SUBDIRECTORY( tests )

CONFIGURE_FILE( ${wibble_SOURCE_DIR}/libwibble.pc.cmake-in ${wibble_BINARY_DIR}/libwibble.pc @ONLY )
CONFIGURE_FILE( ${wibble_SOURCE_DIR}/libwibble-gc.pc.cmake-in ${wibble_BINARY_DIR}/libwibble-gc.pc @ONLY )
INSTALL_FILES( /lib/pkgconfig FILES libwibble.pc libwibble-gc.pc )
install_files( /share/aclocal FILES libwibble.m4 )

IF( XXX_BUILD_TESTING )
  SET( TEST_DIR ${wibble_BINARY_DIR}/tests/work/ )
  ADD_LIBRARY( wibble-testing SHARED ${WSRC} )
  SET_TARGET_PROPERTIES( wibble-testing
                         PROPERTIES DEFINE_SYMBOL WIBBLE_BUILD_TESTING )
  TARGET_LINK_LIBRARIES( wibble-testing )
ENDIF( XXX_BUILD_TESTING )
