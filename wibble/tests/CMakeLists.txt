SET( T sh ${CMAKE_CURRENT_SOURCE_DIR}/libwibble-check )

file( GLOB TEST_SOURCES test_*.cpp )

ADD_CUSTOM_TARGET( check COMMAND make libwibble-test && ${T}
                   DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/data-stamp )

ADD_EXECUTABLE( libwibble-test tut-main.cpp ${TEST_SOURCES} )
TARGET_LINK_LIBRARIES( libwibble-test wibble pthread )

ADD_EXECUTABLE( commandline-demo commandline-demo.cpp )
TARGET_LINK_LIBRARIES( commandline-demo wibble pthread )

#set_target_properties( libwibble-test PROPERTIES LINK_FLAGS ${GC_LINK_FLAGS} )

ADD_CUSTOM_COMMAND(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/data-stamp
  COMMAND touch data-stamp )

#ADD_CUSTOM_COMMAND(
#  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tut-syms.cpp
#  DEPENDS ${wibble_BINARY_DIR}/libwibble.a
#  COMMAND sh ${CMAKE_SOURCE_DIR}/extract_syms.sh ${wibble_BINARY_DIR}/libwibble.a wibble_sym_references > tut-syms.cpp )
