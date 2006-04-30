PROJECT( wibble )

SET( WSRC_TOP
     operators.cpp
     consumer.cpp
     range.cpp
     exception.cpp
     tests.cpp )

SET( WSRC_CMDL
     commandline/core.cpp
     commandline/options.cpp
     commandline/engine.cpp
     commandline/parser.cpp
     commandline/doc.cpp
)

SET( WSRC ${WSRC_TOP} ${WSRC_CMDL} )

INCLUDE_DIRECTORIES( ${wibble_SOURCE_DIR}/.. ${wibble_BINARY_DIR}/.. )
INSTALL_FILES( /include/wibble .h ${WSRC_TOP} )
INSTALL_FILES( /include/wibble/commandline .h ${WSRC_CMDL} )

CONFIGURE_FILE( ${wibble_SOURCE_DIR}/config.h.cmake-in
                ${wibble_BINARY_DIR}/config.h )

IF( BUILD_FINAL )
ADD_LIBRARY( wibble STATIC ${WSRC} )
SET_TARGET_PROPERTIES( wibble PROPERTIES DEFINE_SYMBOL WIBBLE_BUILD_FINAL )
TARGET_LINK_LIBRARIES( wibble gc )
INSTALL_TARGETS( /lib wibble )
ENDIF( BUILD_FINAL )

IF( BUILD_TESTING )
  SET( TEST_DIR ${wibble_BINARY_DIR}/tests/work/ )
  ADD_LIBRARY( wibble-testing SHARED ${WSRC} )
  SET_TARGET_PROPERTIES( wibble-testing
                         PROPERTIES DEFINE_SYMBOL WIBBLE_BUILD_TESTING )
  ADD_SUBDIRECTORY( tests )
  TARGET_LINK_LIBRARIES( wibble-testing gc )
ENDIF( BUILD_TESTING )
