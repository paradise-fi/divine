// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_STDLIBWRAP_HPP__
#define __DIOS_STDLIBWRAP_HPP__

#include <divine.h>
#include <limits>
#include <string>
#include <vector>
#include <set>
#include <map>

namespace __dios {

template < class T >
struct Allocator {
    typedef T value_type;
    typedef size_t size_type;
    typedef int difference_type;

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

    size_t max_size() const {
        return std::numeric_limits< int >::max();
    }
};

using dstring = std::basic_string< char, std::char_traits< char >, Allocator< char >  >;

template < class T >
using dvector = std::vector< T, Allocator< T > >;

template < class T >
using dset = std::set< T, std::less< T >, Allocator< T > >;

template < class K, class V >
using dmap = std::map< K, V, std::less< K >, Allocator< std::pair< const K, V > > >;

} // namespace __dios


#endif // __DIOS_STDLIBWRAP_HPP__
