PROJECT( wibble )

SET( WSRC
     operators.cpp
     consumer.cpp
     range.cpp
     exception.cpp
     commandline/core.cpp
     commandline/options.cpp
     commandline/engine.cpp
     commandline/parser.cpp
     commandline/doc.cpp
     tests.cpp )

SET( WH
     operators.h
     range.h
     consumer.h
)

INCLUDE_DIRECTORIES( ${wibble_SOURCE_DIR}/.. ${wibble_BINARY_DIR}/.. )
INSTALL_FILES( /include/wibble .h ${WH} )

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
