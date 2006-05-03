SET( T "${CMAKE_CURRENT_SOURCE_DIR}/libwibble-check" )

ADD_CUSTOM_TARGET( check COMMAND make libwibble-test && ${T}
                   DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/data-stamp )

ADD_EXECUTABLE( libwibble-test tut-main.cpp tut-syms.cpp )
TARGET_LINK_LIBRARIES( libwibble-test wibble )

ADD_CUSTOM_COMMAND(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/data-stamp
  COMMAND touch data-stamp )

ADD_CUSTOM_COMMAND(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tut-syms.cpp
  DEPENDS ${wibble_BINARY_DIR}/libwibble.a
  COMMAND ${CMAKE_SOURCE_DIR}/extract_syms.sh ${wibble_BINARY_DIR}/libwibble.a wibble_sym_references > tut-syms.cpp )
