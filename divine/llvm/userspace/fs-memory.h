#include <memory>
#include <cstdlib>


#ifndef _FS_MEMORY_H_
#define _FS_MEMORY_H_

namespace divine {
namespace fs {

namespace memory {

template< typename T >
struct Allocator {
    // STL compatibility typedefs
    using value_type = T;
    using pointer = T *;
    using const_pointer = const T *;
    using reference = T &;
    using const_reference = const T &;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

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

    pointer allocate( size_type n, std::allocator< void >::const_pointer hint = nullptr ) {
#ifdef __divine__
        return __divine_malloc( n * sizeof( value_type ) );
#else
        return std::allocator< value_type >().allocate( n, hint );
#endif
    }

    void deallocate( pointer p, size_type n ) {
#ifdef __divine__
        __divine_free( p );
#else
        std::allocator< value_type >().deallocate( p, n );
#endif
    }

    size_type max_size() const {
#ifdef __divine__
        return std::numeric_limits< std::uint32_t >::max();
#else
        return std::numeric_limits< size_type >::max();
#endif
    }

    template< typename U, typename... Args >
    void construct( U *p, Args &&... args ) {
        ::new( p ) U( std::forward< Args >( args )... );
    }

    template< typename U >
    void destroy( U *p ) {
        p->~U();
    }
};

template< typename T1, typename T2 >
bool operator==( const Allocator< T1 > &, const Allocator< T2 > & ) {
    return true;
}

template< typename T1, typename T2 >
bool operator!=( const Allocator< T1 > &, const Allocator< T2 > & ) {
    return false;
}

} // namespace memory

} // namespace fs
} // namespace divine

inline void *operator new( std::size_t count ) {
#ifdef __divine__
    return __divine_malloc( count );
#else
    return std::malloc( count );
#endif
}

inline void *operator new[]( std::size_t count ) {
    return ::operator new( count );
}

inline void operator delete( void *ptr ) noexcept {
#ifdef __divine__
    __divine_free( ptr );
#else
    std::free( ptr );
#endif
}

inline void operator delete[]( void *ptr ) noexcept {
    ::operator delete( ptr );
}

#endif
