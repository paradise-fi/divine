PROJECT( wibble )

SET( WSRC
	 consumer.cpp
	 range.cpp
	 exception.cpp
	 commandline/core.cpp
	 commandline/options.cpp
	 commandline/engine.cpp
	 commandline/parser.cpp
	 commandline/doc.cpp
	 tests.cpp
)

INCLUDE_DIRECTORIES( ${wibble_SOURCE_DIR}/.. ${wibble_BINARY_DIR}/.. )

CONFIGURE_FILE( ${wibble_SOURCE_DIR}/config.h.in
                ${wibble_BINARY_DIR}/config.h )

IF( BUILD_FINAL )
ADD_LIBRARY( wibble STATIC ${WSRC} )
SET_TARGET_PROPERTIES( apt-front
                       PROPERTIES DEFINE_SYMBOL WIBBLE_BUILD_FINAL )
ENDIF( BUILD_FINAL )

IF( BUILD_TESTING )
  SET( TEST_DIR ${wibble_BINARY_DIR}/tests/work/ )
  ADD_LIBRARY( wibble-testing SHARED ${WSRC} )
  SET_TARGET_PROPERTIES( wibble-testing
                         PROPERTIES DEFINE_SYMBOL WIBBLE_BUILD_TESTING )
  ADD_SUBDIRECTORY( tests )
ENDIF( BUILD_TESTING )
