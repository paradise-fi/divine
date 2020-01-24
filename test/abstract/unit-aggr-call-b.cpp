/* TAGS: star min */
/* VERIFY_OPTS: --lamp unit -o nofail:malloc */

#include <sys/lamp.h>

#include <cassert>

char * store_value( char * aggr ) {
    auto val = __lamp_any_i8();
    aggr[5] = val;
    return aggr;
}

int main() {
    auto aggr = static_cast< char * >( __lamp_any_array() );
    auto res = store_value( aggr );

    char arr[ 10 ] = {};
    auto concrete = store_value( arr );
    assert( concrete[ 0 ] == 0 );
    assert( arr[ 5 ] ); /* ERROR */
}
