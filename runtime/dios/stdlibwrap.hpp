// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_STDLIBWRAP_HPP__
#define __DIOS_STDLIBWRAP_HPP__

#include <divine.h>
#include <string>
#include <vector>

namespace __dios {

template < class T >
struct Allocator {
    typedef T value_type;

    T *allocate( std::size_t n ) {
        return static_cast< T * >( __vm_obj_make( n * sizeof( T ) ) );
    }

    void deallocate( T *p, std::size_t ) {
        __vm_obj_free( p );
    }

    template < class U >
    bool operator==( const Allocator<U>& ) {
        return true;
    }

    template <class U>
    bool operator!=( const Allocator<U>& ) {
        return false;
    }
};

using dstring = std::basic_string< char, std::char_traits< char >, Allocator< char >  >;

template < class T >
using dvector = std::vector< T, Allocator< T > >;

} // namespace __dios


#endif // __DIOS_STDLIBWRAP_HPP__
