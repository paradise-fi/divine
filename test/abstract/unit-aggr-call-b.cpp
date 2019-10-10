/* TAGS: star min */
/* VERIFY_OPTS: --symbolic -o nofail:malloc */

#include <rst/domains.h>

#include <cassert>

char * store_value( char * aggr ) {
    auto val = __unit_val_i8();
    aggr[5] = val;
    return aggr;
}

int main() {
    auto aggr = static_cast< char * >( __unit_aggregate() );
    auto res = store_value( aggr );

    char arr[ 10 ] = {};
    auto concrete = store_value( arr );
    assert( concrete[ 0 ] == 0 );
    assert( arr[ 5 ] ); /* ERROR */
}
