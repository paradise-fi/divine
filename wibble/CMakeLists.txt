project( wibble )
include( test.cmake )

# glob through source files/headers
file( GLOB WSRC_TOP *.cpp )
file( GLOB WSRC_CMDL commandline/*.cpp )
file( GLOB WSRC_SYS sys/*.cpp )
file( GLOB WSRC_LOG log/*.cpp )
file( GLOB WSRC_TEXT text/*.cpp )
file( GLOB WSRC_GRCAL grcal/*.cpp )
file( GLOB WSRC_STREAM stream/*.cpp )
file( GLOB H_TOP *.h )
file( GLOB H_CMDL commandline/*.h )
file( GLOB H_SYS sys/*.h )
file( GLOB H_LOG log/*.h )
file( GLOB H_TEXT text/*.h )
file( GLOB H_GRCAL grcal/*.h )
file( GLOB H_STREAM stream/*.h )
file( GLOB H_TESTS tests/*.h )
file( GLOB TCC_TOP *.tcc )
file( GLOB testh *.test.h commandline/*.test.h sys/*.test.h
                 log/*.test.h text/*.test.h grcal/*.test.h stream/*.test.h )

set( WSRC ${WSRC_TOP} ${WSRC_CMDL} ${WSRC_SYS} ${WSRC_LOG} ${WSRC_TEXT} ${WSRC_GRCAL} ${WSRC_STREAM})

# on some mingw32, regex.h is not on the default include path
find_path( RX_PATH regex.h )
include_directories( ${RX_PATH} )

# libwibble.a
if( NOT WIN32 )
set_source_files_properties( ${WSRC} COMPILE_FLAGS -fPIC )
endif( NOT WIN32 )

include_directories( ${wibble_SOURCE_DIR}/.. ${wibble_BINARY_DIR}/.. )
add_definitions( ${OPT_FLAGS} )
add_library( wibble STATIC ${WSRC} )

if( WIN32 )
target_link_libraries( wibble wsock32 psapi regex )
else( WIN32 )
target_link_libraries( wibble pthread )
endif( WIN32 )

if( NOT WIN32 )
# make check
wibble_add_test( wibble-test ${testh} )
target_link_libraries( wibble-test wibble )
wibble_check_target( wibble-test )
endif( NOT WIN32 )

set( prefix "${CMAKE_INSTALL_PREFIX}" )
set( exec_prefix "${prefix}/bin" )
set( libdir "${prefix}/lib" )
set( includedir "${prefix}/include" )

# cmake-time configuration
configure_file( ${wibble_SOURCE_DIR}/libwibble.pc.in
                ${wibble_BINARY_DIR}/libwibble.pc @ONLY )

# make install
install( TARGETS wibble DESTINATION lib COMPONENT wibble_dev )

if( NOT WIN32 )
install( FILES ${wibble_BINARY_DIR}/libwibble.pc DESTINATION lib/pkgconfig COMPONENT wibble_dev )
install( FILES libwibble.m4 DESTINATION share/aclocal COMPONENT wibble_dev )
install( FILES wibble-test-genrunner.1 DESTINATION share/man/man1 COMPONENT wibble_dev )
endif( NOT WIN32 )

install( FILES ${H_TOP} DESTINATION include/wibble COMPONENT wibble_dev )
install( FILES ${TCC_TOP} DESTINATION include/wibble COMPONENT wibble_dev )
install( FILES ${H_CMDL} DESTINATION include/wibble/commandline COMPONENT wibble_dev )
install( FILES ${H_SYS} DESTINATION include/wibble/sys COMPONENT wibble_dev )
install( FILES ${H_LOG} DESTINATION include/wibble/log COMPONENT wibble_dev )
install( FILES ${H_TEXT} DESTINATION include/wibble/text COMPONENT wibble_dev )
install( FILES ${H_GRCAL} DESTINATION include/wibble/grcal COMPONENT wibble_dev )
install( FILES ${H_STREAM} DESTINATION include/wibble/stream COMPONENT wibble_dev )
install( FILES ${H_TESTS} DESTINATION include/wibble/tests COMPONENT wibble_dev )

if( NOT WIN32 )
install( FILES test.cmake DESTINATION share/wibble COMPONENT wibble_dev )
install( FILES test-genrunner.pl
  RENAME wibble-test-genrunner
  DESTINATION bin COMPONENT wibble_dev 
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE )
endif( NOT WIN32 )
