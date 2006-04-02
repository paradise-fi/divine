SET( T "${CMAKE_CURRENT_SOURCE_DIR}/libwibble-check" )

ADD_CUSTOM_TARGET( check COMMAND make libwibble-test && ${T}
                   DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/data-stamp )

ADD_EXECUTABLE( libwibble-test tut-main.cpp )
TARGET_LINK_LIBRARIES( libwibble-test wibble-testing )

ADD_CUSTOM_COMMAND(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/data-stamp
  COMMAND touch data-stamp )
