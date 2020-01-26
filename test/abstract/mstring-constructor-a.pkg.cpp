/* TAGS: mstring todo */
/* VERIFY_OPTS: -o nofail:malloc */

// V: con V_OPT: --lamp constring            TAGS: min
// V: sym V_OPT: --lamp symstring --symbolic TAGS: sym

#include <sys/lamp.h>

#include <assert.h>
#include <stdlib.h>

int main() {
    {
        char * str = __mstring_sym_val( 3, 'a', 1, 2, 'b', 3, 5, '\0', 6, 7 );
        assert( str[ 0 ] == 'a' );
        assert( str[ 2 ] == 'a' || str[ 2 ] == 'b' );
        assert( str[ 4 ] == 'b' || str[ 4 ] == '\0' );
        assert( str[ 5 ] == '\0' );
        free( str );
    }
}
