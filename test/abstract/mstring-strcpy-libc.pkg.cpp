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

static constexpr size_t length = 3;

int main()
{
    char * abuff = sym::buffer( length + 1 );
    char * mbuff = sym::buffer( length + 1 );
    char * str = sym::string( length );

    strcpy( abuff, str );
    libc::strcpy( mbuff, str );

    assert( strcmp( abuff, mbuff ) == 0 );

    free( abuff );
    free( mbuff );
    free( str );
}
