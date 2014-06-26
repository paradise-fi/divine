include( ExternalProject )

file( GLOB _BINUTILS_SRC RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} binutils-* )
if( NOT _BINUTILS_SRC )
    message( FATAL_ERROR "Binutils missing. Please download binutils and extact them into <divine source directory>/external/binutils/." )
endif()

set( BINUTILS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/${_BINUTILS_SRC}" )

message( "BINUTILS: ${BINUTILS_SRC}" )

ExternalProject_Add(
    binutils
    SOURCE_DIR ${BINUTILS_SRC}
    CONFIGURE_COMMAND ${BINUTILS_SRC}/configure --prefix=${CMAKE_BINARY_DIR}/binutils --disable-ld --enable-gold --enable-plugins --program-prefix=divine-
    BUILD_COMMAND make
    BUILD_IN_SOURCE 0
    INSTALL_COMMAND make install
)
