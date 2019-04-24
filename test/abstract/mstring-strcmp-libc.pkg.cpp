/* TAGS: mstring min sym c++ */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
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

template < typename T > int sgn( T val ) {
    return ( T(0) < val ) - ( val < T(0) );
}

int main()
{
    char * left = sym::string( length );
    char * right = sym::string( length );

    assert( sgn( strcmp( left, right ) ) == sgn( libc::strcmp( left, right ) ) );

    free( left );
    free( right );
}
