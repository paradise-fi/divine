#pragma once

#include <cassert>
#include <sys/lamp.h>

namespace sym {

static inline char * buffer( uint32_t size ) {
    return __mstring_from_segments( 0, size );
}

#ifdef WORD
static inline char * string( uint32_t size ) {
    return __mstring_from_segments( 4, size );
}
#endif

#ifdef SEQUENCE
static inline char * string( uint32_t size ) {
    return __mstring_from_segments( 1, size );
}
#endif

#ifdef ALTERNATION
static inline char * string( uint32_t size ) {
    return __mstring_from_segments( 2, size );
}
#endif

} // namespace sym

