// -*- C++ -*- (c) 2015 Jiří Weiser
//             (c) 2014 Vladimír Štill

/*
 * Few things ported from brick-fs and brick-types.
 */
#include <utility>
#include <algorithm>
#include <type_traits>
#include <cerrno>

#ifndef _FS_PATH_UTILS_H_
#define _FS_PATH_UTILS_H_

#ifdef __divine__
#include <sys/divm.h>
#include <dios/core/stdlibwrap.hpp>
#include <dios/core/syscall.hpp>
#include <dios/kernel.hpp>

#define __FS_assert( x ) do { \
        if ( !(x) ) { \
            __vm_trace(  _VM_T_Text, "FS assert");\
            __dios_fault( _VM_Fault::_VM_F_Assert, "FS assert" );\
        } \
    } while (0)

# define FS_ATOMIC_SECTION_BEGIN()
# define FS_ATOMIC_SECTION_END()
# define FS_CHOICE( n )             __vm_choose( n )


#else
# include "sys/divm.h"
enum Problems {
    Other = 2
};

# define FS_INTERRUPT()
# define FS_ATOMIC_SECTION_BEGIN()
# define FS_ATOMIC_SECTION_END()
# define FS_CHOICE( n )             FS_CHOICE_GOAL
#endif


namespace __dios {
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
    return new( __dios::nofail ) Type( std::forward< Args >( args )... );
}

template< typename Type, typename... Args >
auto constructIfPossible( Args &&... )
    -> typename std::enable_if< !std::is_constructible< Type, Args... >::value, Type * >::type
{
    return nullptr;
}

} // namespace utils

struct Error {
    Error( int code ) : _code( code ) {
    }

    int code() const noexcept {
        return _code;
    }
private:
    int _code;
};

} // namespace fs
} // namespace __dios

#endif
