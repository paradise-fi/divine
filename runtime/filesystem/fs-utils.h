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

# define FS_INTERRUPT()             /*__divine_interrupt()*/
# define FS_ATOMIC_SECTION_BEGIN()  /*__divine_interrupt_mask()*/
# define FS_ATOMIC_SECTION_END()    /*__divine_interrupt_unmask()*/

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

template< typename T >
struct Adaptor {
    Adaptor( T begin, T end ) :
        _begin( begin ),
        _end( end ) {}

    T begin() {
        return _begin;
    }
    T begin() const {
        return _begin;
    }
    T end() {
        return _end;
    }
    T end() const {
        return _end;
    }

private:
    T _begin;
    T _end;
};

template< typename T >
auto withOffset( T &&container, size_t offset )
    -> Adaptor< decltype( container.begin() ) >
{
    using Iterator = decltype( container.begin() );
    return Adaptor< Iterator >( container.begin() + offset, container.end() );
}

template< typename E >
using is_enum_class = std::integral_constant< bool,
        std::is_enum< E >::value && !std::is_convertible< E, int >::value >;


template< typename Type, typename... Args >
auto constructIfPossible( Args &&... args )
    -> typename std::enable_if< std::is_constructible< Type, Args... >::value, Type * >::type
{
    return new( memory::nofail ) Type( std::forward< Args >( args )... );
}

template< typename Type, typename... Args >
auto constructIfPossible( Args &&... args )
    -> typename std::enable_if< !std::is_constructible< Type, Args... >::value, Type * >::type
{
    return nullptr;
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
