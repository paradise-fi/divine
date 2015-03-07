// -*- C++ -*- (c) 2015 Jiří Weiser

#include "fs-memory.h"

void *operator new( std::size_t count ) {
#ifdef __divine__
    return __divine_malloc( count );
#else
    return std::malloc( count );
#endif
}

void *operator new[]( std::size_t count ) {
    return ::operator new( count );
}

void operator delete( void *ptr ) noexcept {
#ifdef __divine__
    __divine_free( ptr );
#else
    std::free( ptr );
#endif
}

void operator delete[]( void *ptr ) noexcept {
    ::operator delete( ptr );
}
