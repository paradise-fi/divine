// -*- C++ -*- (c) 2015 Jiří Weiser
//             (c) 2014 Vladimír Štill

/*
 * Few things ported from brick-fs and brick-types.
 */
#include <vector>
#include <string>
#include <queue>
#include <set>
#include <list>
#include <utility>
#include <algorithm>
#include <type_traits>
#include <cerrno>

#include "fs-memory.h"

#ifndef _FS_PATH_UTILS_H_
#define _FS_PATH_UTILS_H_

#ifdef __divine__
# include <divine.h>
# include <divine/problem.h>

# define FS_INTERRUPT()             __divine_interrupt()
# define FS_ATOMIC_SECTION_BEGIN()  __divine_interrupt_mask()
# define FS_ATOMIC_SECTION_END()    __divine_interrupt_unmask()

#else
# include "divine.h"
enum Problems {
    Other = 2
};

# define FS_INTERRUPT()
# define FS_ATOMIC_SECTION_BEGIN()
# define FS_ATOMIC_SECTION_END()
#endif

#define FS_BREAK_MASK( command )            \
    do {                                    \
        FS_ATOMIC_SECTION_END();            \
        command;                            \
        FS_ATOMIC_SECTION_BEGIN();          \
    } while( false )

#define FS_MAKE_INTERRUPT()                 \
    FS_BREAK_MASK( FS_INTERRUPT() )


namespace divine {
namespace fs {
namespace utils {

template< typename T >
struct Defer {
    Defer( T &&c ) : _c( std::move( c ) ), _deleted( false ) {}
    ~Defer() {
        run();
    }

    void run() {
        if ( !_deleted ) {
            _c();
            _deleted = true;
        }
    }

    bool deleted() const {
        return _deleted;
    }

    void pass() {
        _deleted = true;
    }

private:
    T _c;
    bool _deleted;
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


using String = std::basic_string< char, std::char_traits< char >, memory::Allocator< char > >;

template< typename T >
using Vector = std::vector< T, memory::Allocator< T > >;

template< typename T >
using Deque = std::deque< T, memory::Allocator< T > >;

template< typename T >
using Queue = std::queue< T, Deque< T > >;

template< typename T >
using Set = std::set< T, std::less< T >, memory::Allocator< T > >;

template< typename T >
using List = std::list< T, memory::Allocator< T > >;

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

#endif
