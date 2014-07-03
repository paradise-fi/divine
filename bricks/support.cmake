macro( bricks_unittest name )
  set( main "${CMAKE_CURRENT_BINARY_DIR}/${name}-main.cpp" )
  if( EXISTS ${main} )
    file( READ ${main} old )
  endif()

  foreach( src ${ARGN} )
    set( new "${new}\n#include <${src}>" )
  endforeach( src )

  set( new "${new}
    int main( int argc, const char **argv ) {
      brick::unittest::run( argc > 1 ? argv[1] : \"\",
                            argc > 2 ? argv[2] : \"\" )\;
    }"
  )

  if( NOT "${old}" STREQUAL "${new}" )
    file( WRITE ${main} ${new} )
  endif()

  add_executable( ${name} ${main} )
  set_source_files_properties( ${main} PROPERTIES COMPILE_DEFINITIONS UNITTEST )
endmacro()
