/* TAGS: mstring todo sym c++ */
/* VERIFY_OPTS: --sequential -o nofail:malloc */

// V: con V_OPT: --lamp constring            TAGS: min
// V: sym V_OPT: --lamp symstring --symbolic TAGS: sym
/* CC_OPTS: */

// V: v.word CC_OPT: -DWORD
// V: v.sequence CC_OPT: -DSEQUENCE
// V: v.alternation CC_OPT: -DALTERNATION

// V: v.word-leakcheck CC_OPT: -DWORD VERIFY_OPTS: --leakcheck exit TAGS: todo
// V: v.sequence-leakcheck CC_OPT: -DSEQUENCE VERIFY_OPTS: --leakcheck exit TAGS: todo
// V: v.alternation-leakcheck CC_OPT: -DALTERNATION VERIFY_OPTS: --leakcheck exit TAGS: todo

#include "common.h"
#include "libc.hpp"

#include <cstring>
#include <cstdlib>

static constexpr size_t length = 2;

int main()
{
    char * left  = sym::string( length );
    char * right = sym::string( length );

    char * abuff = sym::buffer( length * 2 + 1 );
    char * mbuff = sym::buffer( length * 2 + 1 );

    strcat( abuff, left );
    strcat( abuff, right );

    libc::strcat( mbuff, left );
    libc::strcat( mbuff, right );

    assert( strcmp( abuff, mbuff ) == 0 );

    free( abuff );
    free( mbuff );
    free( left );
    free( right );
}
