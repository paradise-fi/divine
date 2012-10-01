# -I/usr/include/nptl -static -malign-double -           pipe -march=pentium4 -O3 -finline-limit=20000 -fomit-frame-pointer -finline-        functions  -DNDEBUG  -I. -Iheaplayers -Iheaplayers/util -D_REENTRANT=1 -            shared libhoard.cpp -Bsymbolic -o libhoard.so -ldl -lpthread

macro( reset VAR )
set( ${VAR} "${VAR}" )
endmacro( reset VAR )

add_definitions( ${OPT_FLAGS} ${CXX_FLAGS} -static -Bsymbolic )

include( CheckCXXCompilerFlag )
reset( HAVE_MALIGN_DOUBLE )
check_cxx_compiler_flag( "-malign-double" HAVE_MALIGN_DOUBLE )

if( ${HAVE_MALIGN_DOUBLE} )
  add_definitions( -malign-double )
endif( ${HAVE_MALIGN_DOUBLE} )

if( OPTIMIZE )
  add_definitions( -finline-functions -finline-limit=20000 )
endif( OPTIMIZE )

include_directories( /usr/include/nptl heaplayers heaplayers/util )
link_libraries( dl pthread )

add_library( hoard STATIC libhoard.cpp )
