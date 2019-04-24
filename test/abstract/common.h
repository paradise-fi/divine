#pragma once

#include <cassert>
#include <rst/domains.h>

namespace sym {

static inline char * buffer( int size ) {
    return __mstring_sym_val( 1, '\0', size, size + 1 );
}

#ifdef WORD
static inline char * string( int size ) {
    return __mstring_sym_val( 4,
        'a', 1, size,
        'b', 1, size,
        'c', 1, size,
        '\0', size + 1, size + 2 );
}
#endif

#ifdef SEQUENCE
static inline char * string( int size ) {
    return __mstring_sym_val( 2,
        'a', 1, size,
        '\0', size + 1, size + 2 );
}
#endif

#ifdef ALTERNATION
static inline char * string( int size ) {
    return __mstring_sym_val( 5,
        'a', 1, size,
        'b', 1, size,
        'a', 1, size,
        'b', 1, size,
        '\0', size + 1, size + 2 );
}
#endif

} // namespace sym

