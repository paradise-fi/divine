// -*- C++ -*- (c) 2015 Jiří Weiser
//             (c) 2016 Jan Mrázek <email@honzamrazek.cz>

#include <memory>
#include <cstdlib>
#include <limits>

#include "sys/divm.h"

#ifndef _DIOS_MEMORY_H_
#define _DIOS_MEMORY_H_

#if __cplusplus >= 201103L
#define NOTHROW noexcept
#else
#define NOTHROW throw()
#endif

namespace __dios {

struct MemoryPool {
    MemoryPool( int size ) noexcept :
        _pos( 0 ),
        _pool( static_cast< void ** >( __vm_obj_make( size * sizeof( void * ) ) ) )
    {
        for ( int i = 0; i != size; i++ ) {
            _pool[ i ] = __vm_obj_make( 1 );
        }
    }

    MemoryPool( const MemoryPool& ) = delete;
    MemoryPool& operator=( const MemoryPool& ) = delete;

    ~MemoryPool()
    {
        for ( int i = _pos; i < __vm_obj_size( _pool ) / sizeof( void * ); ++i )
            __vm_obj_free( _pool[ i ] );
        __vm_obj_free( _pool );
    }

    void *get() noexcept
    {
        if ( __vm_obj_size( _pool ) != sizeof( void * ) * _pos )
            return _pool[ _pos++ ];
        return nullptr;
    }

    int _pos;
    void **_pool;
};

struct AllocatorBase {

    using pointer = void *;
    using const_pointer = const void *;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    AllocatorBase() {}
    AllocatorBase( const AllocatorBase & ) {};
    template< typename U >
    AllocatorBase( const std::allocator< U > & ) {}

    pointer allocate( size_type n, __attribute__((unused)) const_pointer hint = nullptr ) {
#ifdef __divine__
        return static_cast< pointer >( __vm_obj_make( n ) );
#else
        return std::allocator< char >().allocate( n, hint );
#endif
    }

    void deallocate( pointer p, __attribute__((unused)) size_type n ) {
#ifdef __divine__
        __vm_obj_free( p );
#else
        char *_p = static_cast< char * >( p );
        std::allocator< char >().deallocate( _p, n );
#endif
    }

    size_type max_size() const {
#ifdef __divine__
        return std::numeric_limits< std::int32_t >::max();
#else
        return std::numeric_limits< size_type >::max();
#endif
    }

    template< typename U, typename... Args >
    void construct( U *p, Args &&... args ) {
        ::new( (void *)p ) U( std::forward< Args >( args )... );
    }

    template< typename U >
    void destroy( U *p ) {
        p->~U();
    }
};

template< typename T >
struct Allocator : AllocatorBase {
    // STL compatibility typedefs
    using value_type = T;
    using pointer = T *;
    using const_pointer = const T *;
    using reference = T &;
    using const_reference = const T &;

    // using propagate_on_container_move_assignment = std::true_type; // C++14
    // using is_always_equal = std::true_type; // C++17

    Allocator() {}
    Allocator( const Allocator & ) {};
    template< typename U >
    Allocator( const Allocator< U > & ) {}
    template< typename U >
    Allocator( const std::allocator< U > & ) {}

    template< typename U >
    struct rebind {
        using other = Allocator< U >;
    };

    pointer address( reference x ) const {
        return &x;
    }
    const_pointer address( const_reference x ) const {
        return &x;
    }

    pointer allocate( size_type n, const_pointer hint = nullptr ) {
        return static_cast< pointer >( AllocatorBase::allocate( n * sizeof( value_type ), hint ) );
    }

    void deallocate( pointer p, size_type n ) {
        AllocatorBase::deallocate( p, n * sizeof( value_type ) );
    }

};

using AllocatorPure = Allocator< char >;

template< typename T1, typename T2 >
bool operator==( const Allocator< T1 > &, const Allocator< T2 > & ) {
    return true;
}

template< typename T1, typename T2 >
bool operator!=( const Allocator< T1 > &, const Allocator< T2 > & ) {
    return false;
}

struct nofail_t {};
extern nofail_t nofail;

} // namespace __dios

void *operator new( std::size_t count, const __dios::nofail_t & ) NOTHROW;
void *operator new[]( std::size_t count, const __dios::nofail_t & ) NOTHROW;

#endif // _DIOS_MEMORY_H_
