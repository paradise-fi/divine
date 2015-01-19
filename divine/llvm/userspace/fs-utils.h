/**
 * Few things ported from brick-fs and brick-types.
 */
#include <vector>
#include <string>
#include <queue>
#include <set>
#include <utility>
#include <algorithm>
#include <type_traits>
#include <cerrno>

#ifndef _FS_PATH_UTILS_H_
#define _FS_PATH_UTILS_H_

namespace divine {
namespace fs {
namespace utils {

template< typename T >
struct Defer {
    Defer( T &&c ) : _c( std::move( c ) ), _run( false ) {}
    ~Defer() {
        run();
    }

    void run() {
        if ( !_run ) {
            _c();
            _run = true;
        }
    }

private:
    T _c;
    bool _run;
};

template< typename T >
Defer< T > make_defer( T &&c ) {
    return Defer< T >( std::move( c ) );
}

template< typename E >
using is_enum_class = std::integral_constant< bool,
        std::is_enum< E >::value && !std::is_convertible< E, int >::value >;

template< typename Self >
struct StrongEnumFlags {
    static_assert( is_enum_class< Self >::value, "Not an enum class." );
    using This = StrongEnumFlags< Self >;
    using UnderlyingType = typename std::underlying_type< Self >::type;

    constexpr StrongEnumFlags() noexcept : store( 0 ) { }
    constexpr StrongEnumFlags( Self flag ) noexcept :
        store( static_cast< UnderlyingType >( flag ) )
    { }
    explicit constexpr StrongEnumFlags( UnderlyingType st ) noexcept : store( st ) { }

    constexpr explicit operator UnderlyingType() const noexcept {
        return store;
    }

    This &operator|=( This o ) noexcept {
        store |= o.store;
        return *this;
    }

    This &operator&=( This o ) noexcept {
        store &= o.store;
        return *this;
    }

    This &operator^=( This o ) noexcept {
        store ^= o.store;
        return *this;
    }

    friend constexpr This operator|( This a, This b ) noexcept {
        return This( a.store | b.store );
    }

    friend constexpr This operator&( This a, This b ) noexcept {
        return This( a.store & b.store );
    }

    friend constexpr This operator^( This a, This b ) noexcept {
        return This( a.store ^ b.store );
    }

    friend constexpr bool operator==( This a, This b ) noexcept {
        return a.store == b.store;
    }

    friend constexpr bool operator!=( This a, This b ) noexcept {
        return a.store != b.store;
    }

    constexpr bool has( Self x ) const noexcept {
        return ((*this) & x) == x;
    }

    This clear( Self x ) noexcept {
        store &= ~UnderlyingType( x );
        return *this;
    }

    explicit constexpr operator bool() const noexcept {
        return store;
    }

  private:
    UnderlyingType store;
};

// don't catch integral types and classical enum!
template< typename Self, typename = typename
          std::enable_if< is_enum_class< Self >::value >::type >
constexpr StrongEnumFlags< Self > operator|( Self a, Self b ) noexcept {
    using Ret = StrongEnumFlags< Self >;
    return Ret( a ) | Ret( b );
}

template< typename Self, typename = typename
          std::enable_if< is_enum_class< Self >::value >::type >
constexpr StrongEnumFlags< Self > operator&( Self a, Self b ) noexcept {
    using Ret = StrongEnumFlags< Self >;
    return Ret( a ) & Ret( b );
}

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

using String = std::basic_string< char, std::char_traits< char >, Allocator< char > >;

template< typename T >
using Vector = std::vector< T, Allocator< T > >;

template< typename T >
using Deque = std::deque< T, Allocator< T > >;

template< typename T >
using Queue = std::queue< T, Deque< T > >;

template< typename T >
using Set = std::set< T, std::less< T >, memory::Allocator< T > >;

} // namespace utils

struct Error {
    Error( int code ) : _code( code ) {
        errno = code;
    }

    int code() const noexcept {
        return _code;
    }
private:
    int _code;
};

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
