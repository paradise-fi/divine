project( wibble )
include( test.cmake )

# glob through source files/headers
file( GLOB WSRC_TOP *.cpp )
file( GLOB WSRC_CMDL commandline/*.cpp )
file( GLOB WSRC_SYS sys/*.cpp )
file( GLOB WSRC_LOG log/*.cpp )
file( GLOB WSRC_TEXT text/*.cpp )
file( GLOB H_TOP *.h )
file( GLOB H_CMDL commandline/*.h )
file( GLOB H_SYS sys/*.h )
file( GLOB H_LOG log/*.h )
file( GLOB H_TEXT text/*.h )
file( GLOB TCC_TOP *.tcc )
file( GLOB testh *.test.h commandline/*.test.h sys/*.test.h
                 log/*.test.h text/*.test.h )

set( WSRC ${WSRC_TOP} ${WSRC_CMDL} ${WSRC_SYS} ${WSRC_LOG} ${WSRC_TEXT} )

# libwibble.a
set_source_files_properties( ${WSRC} COMPILE_FLAGS -fPIC )
include_directories( ${wibble_SOURCE_DIR}/.. ${wibble_BINARY_DIR}/.. )
add_definitions( ${OPT_FLAGS} )
add_library( wibble STATIC ${WSRC} )
target_link_libraries( wibble pthread )

# make check
wibble_add_test( wibble-test ${testh} )
target_link_libraries( wibble-test wibble )
wibble_check_target( wibble-test )

# cmake-time configuration
configure_file( ${wibble_SOURCE_DIR}/libwibble.pc.in
                ${wibble_BINARY_DIR}/libwibble.pc @ONLY )

# make install
install( TARGETS wibble DESTINATION lib )
install( FILES ${wibble_BINARY_DIR}/libwibble.pc DESTINATION lib/pkgconfig )
install( FILES libwibble.m4 DESTINATION share/aclocal )

install( FILES ${H_TOP} DESTINATION include/wibble )
install( FILES ${TCC_TOP} DESTINATION include/wibble )
install( FILES ${H_CMDL} DESTINATION include/wibble/commandline )
install( FILES ${H_SYS} DESTINATION include/wibble/sys )
install( FILES ${H_LOG} DESTINATION include/wibble/log )
install( FILES ${H_TEXT} DESTINATION include/wibble/text )
