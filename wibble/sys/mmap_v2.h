// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>
/* mmap support using C++11
 *
 * mmaped file can be shared accross threads without memory overhead,
 * but obviously it is not therad safe. It has shared_ptr semantics.
 *
 * redistributable under BSD licence
 */

#if __cplusplus < 201103L
#error "mmap_v2 is only supported with c++11 or newer"
#endif

#include <wibble/strongenumflags.h>

#include <memory>
#include <string>

#ifndef WIBBLE_SYS_MMAP_V2
#define WIBBLE_SYS_MMAP_V2

namespace wibble {
namespace sys {
inline namespace v2 {

struct MMap
{
    enum class ProtectMode {
        Read = 0x1, Write = 0x2, Execute = 0x4,
        Shared = 0x8, Private = 0x10
    };
#define DEFAULT_MODE (ProtectMode::Read | ProtectMode::Shared)
    using ProtectModeFlags = StrongEnumFlags< ProtectMode >;

    MMap() : _size( 0 ) { }
    MMap( const std::string &, ProtectModeFlags = DEFAULT_MODE );
    MMap( int fd, ProtectModeFlags );

    void map( const std::string &, ProtectModeFlags = DEFAULT_MODE );
    void map( int fd, ProtectModeFlags = DEFAULT_MODE );
    void unmap();

#undef DEFAULT_MODE

    size_t size() { return _size; }
    explicit operator bool() { return bool( _ptr ); }
    bool valid() { return bool( _ptr ); }
    ProtectModeFlags mode() { return _flags; }

    // get value on begining offset bites
    template< typename T >
    T &get( size_t offset ) {
        return *reinterpret_cast< T * >(
                reinterpret_cast< char * >( _ptr.get() ) + offset );
    }

    template< typename T >
    const T &cget( size_t offset ) const {
        return *reinterpret_cast< T * >(
                reinterpret_cast< char * >( _ptr.get() ) + offset );
    }

    template< typename T >
    const T &get( size_t offset ) const { return cget< T >( offset ); }

    template< typename T >
    T *asArrayOf() {
        return reinterpret_cast< T * >( _ptr.get() );
    }

    template< typename T >
    const T *asConstArrayOf() const {
        return reinterpret_cast< const T * >( _ptr.get() );
    }

    template< typename T >
    const T *asArrayOf() const {
        return asConstArrayOf< T >();
    }

    char &operator[]( size_t offset ) {
        return asArrayOf< char >()[ offset ];
    }

    const char &operator[]( size_t offset ) const {
        return asArrayOf< char >()[ offset ];
    }

  private:
    std::shared_ptr< void > _ptr;
    ProtectModeFlags _flags;
    size_t _size;
    void _map( int );
    void _map( const std::string & );
};

}
}
}

#endif // WIBBLE_SYS_MMAP_V2
