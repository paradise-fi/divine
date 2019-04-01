/* TAGS: mstring min sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */

#include <rst/domains.h>
#include <cassert>
#include <cstring>
#include <cstdlib>

char * string( int size ) {
    return __mstring_sym_val( 4, 'a', 1, size, 'b', 1, size, 'c', 1, size + 1, '\0', size + 1, size + 2 );
}

namespace musl {
    int strcmp(const char *l, const char *r)
    {
        for (; *l==*r && *l; l++, r++);
        return *(unsigned char *)l - *(unsigned char *)r;
    }
} // namespace musl

template < typename T > int sgn( T val ) {
    return ( T(0) < val ) - ( val < T(0) );
}

int main()
{
    char * left = string( 3 );
    char * right = string( 3 );

    assert( sgn( strcmp( left, right ) ) == sgn( musl::strcmp( left, right ) ) );

    free( left );
    free( right );
}
