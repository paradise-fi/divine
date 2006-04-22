/** -*- C++ -*-
*/

#include <gc/gc_cpp.h>
#include <gc/gc_allocator.h>

#ifndef WIBBLE_SHARED_H
#define WIBBLE_SHARED_H

namespace wibble {

template< typename T >
struct SharedVector : gc, std::vector< T, gc_allocator< T > >
{};

template< typename T >
T &shared() {
    return *(new (GC) T());
}

}

#endif
// kate: indent-width 4; replace-tabs on; tab-width 4; space-indent on;
