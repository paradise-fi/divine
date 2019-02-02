// -*- C++ -*- (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#ifndef __DIOS_STDLIBWRAP_HPP__
#define __DIOS_STDLIBWRAP_HPP__

#include <sys/divm.h>
#include <vector>
#include <string>
#include <queue>
#include <set>
#include <list>
#include <map>

#include <dios/sys/memory.hpp>

namespace __dios {

using String = std::basic_string< char, std::char_traits< char >, Allocator< char > >;

template< typename T >
using Vector = std::vector< T, Allocator< T > >;

template< typename T >
using Deque = std::deque< T, Allocator< T > >;

template< typename T >
using Set = std::set< T, std::less< T >, Allocator< T > >;

template< typename T >
using List = std::list< T, Allocator< T > >;

template< typename T >
using Queue = std::queue< T, List< T > >;

template < class K, class V >
using Map = std::map< K, V, std::less< K >, Allocator< std::pair< const K, V > > >;

template < class T, class... Args >
T *new_object( Args... args ) {
    T* obj = static_cast< T * >( __vm_obj_make( sizeof( T ) ?: 1, _VM_PT_Heap ) );
    new (obj) T( args... );
    return obj;
}

template < class T >
void delete_object( T *obj ) {
    obj->~T();
    __vm_obj_free( obj );
}

} // namespace __dios


#endif // __DIOS_STDLIBWRAP_HPP__
