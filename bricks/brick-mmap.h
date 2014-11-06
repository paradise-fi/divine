// -*- C++ -*- (c) 2013 Vladimír Štill <xstill@fi.muni.cz>
/* mmap support using C++11
 *
 * mmaped file can be shared across threads without memory overhead,
 * but obviously it is not thread safe. It has shared_ptr semantics.
 *
 * redistributable under BSD licence
 */

#if __cplusplus < 201103L
#error "brick-mmap is only supported with c++11 or newer"
#endif

#ifdef __unix

#include <memory>
#include <string>
#include <stdexcept>

#include <type_traits>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include <brick-assert.h>
#include <brick-types.h>

#ifndef BRICK_MMAP_H
#define BRICK_MMAP_H

namespace brick {
namespace mmap {

struct SystemException : std::exception {
    // make sure we don't override errno accidentaly when constructing std::string
    explicit SystemException( std::string what ) : SystemException{ errno, what } { }
    explicit SystemException( const char *what ) : SystemException{ errno, what } { }

    virtual const char *what() const noexcept override { return _what.c_str(); }

  private:
    std::string _what;

    explicit SystemException( int _errno, std::string what ) {
        _what = "System error: " + std::string( std::strerror( _errno ) ) + ", when " + what;
    }
};

enum class ProtectMode {
    Read = 0x1, Write = 0x2, Execute = 0x4,
    Shared = 0x8, Private = 0x10
};

using ::brick::types::operator|;


struct MMap
{
    using ProtectModeFlags = ::brick::types::StrongEnumFlags< ProtectMode >;
#define DEFAULT_MODE (ProtectMode::Read | ProtectMode::Shared)

    MMap() : _size( 0 ) { }
    MMap( const std::string &file, ProtectModeFlags flags = DEFAULT_MODE ) :
        _flags( flags ), _size( 0 )
    {
        _map( file );
    }

    MMap( int fd, ProtectModeFlags flags ) : _flags( flags ), _size( 0 ) {
        _map( fd );
    }

    void map( const std::string &file, ProtectModeFlags flags = DEFAULT_MODE ) {
        _flags = flags;
        _map( file );
    }

    void map( int fd, ProtectModeFlags flags = DEFAULT_MODE ) {
        _flags = flags;
        _map( fd );
    }

    void unmap() {
        _ptr = nullptr;
        _size = 0;
    }

#undef DEFAULT_MODE

    size_t size() { return _size; }
    explicit operator bool() { return bool( _ptr ); }
    bool valid() { return bool( _ptr ); }
    ProtectModeFlags mode() { return _flags; }

    // get value on beginning offset bites
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

    const char *cdata() const { return asConstArrayOf< char >(); }
    const char *data() const { return asConstArrayOf< char >(); }
    char *data() { return asArrayOf< char >(); }

  private:
    std::shared_ptr< void > _ptr;
    ProtectModeFlags _flags;
    size_t _size;

    void _map( int fd ) {
        struct stat st;
        if ( fstat( fd, &st ) != 0 )
            throw SystemException( "stat failed while mmaping" );
        size_t size = _size = st.st_size;

        void *ptr = ::mmap( nullptr, _size, _mmapProt(), _mmapFlags(), fd, 0 );
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
        if ( ptr == MAP_FAILED )
            throw SystemException( "mmaping file failed" );
#pragma GCC diagnostic pop

        _ptr = std::shared_ptr< void >( ptr,
            [ fd, size ]( void *h ) {
                ::munmap( h, size );
                ::close( fd );
            } );
    }

    void _map( const std::string &file ) {
        int fd = ::open( file.c_str(), _openFlags() );
        if ( fd < 0 )
            throw SystemException( "opening file failed: " + file );
        _map( fd );
    }

    int _mmapProt() {
        int prot = 0;
        if ( _flags.has( ProtectMode::Read ) ) prot |= PROT_READ;
        if ( _flags.has( ProtectMode::Write ) ) prot |= PROT_WRITE;
        if ( _flags.has( ProtectMode::Execute ) ) prot |= PROT_EXEC;
        return prot;
    }

    int _mmapFlags() {
        int mf = 0;
        if ( _flags.has( ProtectMode::Shared ) ) mf |= MAP_SHARED;
        if ( _flags.has( ProtectMode::Private ) ) mf |= MAP_PRIVATE;
        return mf;
    }

    int _openFlags() {
        if ( _flags.has( ProtectMode::Read ) &&
                _flags.has( ProtectMode::Write ) )
            return O_RDWR;
        if ( _flags.has( ProtectMode::Read ) )
            return O_RDONLY;
        if ( _flags.has( ProtectMode::Write ) )
            return O_WRONLY;
        ASSERT_UNREACHABLE( "No open flags specified" );
    }
};

}
}

/* unit tests */

namespace brick_test {
namespace mmap {

using namespace ::brick::mmap;

struct MMapTest {
    TEST(read) {
#if defined POSIX && __cplusplus >= 201103L
        MMap map;
        ASSERT_EQ( map.size(), 0U );
        ASSERT( !map );
        ASSERT( !map.valid() );
        ASSERT( !map.mode() );

        map.map( "/bin/sh" );
        ASSERT_NEQ( map.size(), 0U );
        ASSERT_EQ( map.mode(), ProtectMode::Read | ProtectMode::Shared );
        ASSERT( map.valid() );
        ASSERT_EQ( map[ 1 ], 'E' );
        ASSERT_EQ( map[ 2 ], 'L' );
        ASSERT_EQ( map[ 3 ], 'F' );

        MMap map1 = map; // shared_ptr semantics
        ASSERT_EQ( map.size(), map.size() );
        ASSERT_EQ( map.asArrayOf< char >(), map1.asArrayOf< char >() );
        ASSERT_EQ( map.mode(), map1.mode() );

        ASSERT_EQ( map1.get< char >( 1 ), 'E' );
        ASSERT_EQ( map1.get< char >( 2 ), 'L' );
        ASSERT_EQ( map1.get< char >( 3 ), 'F' );

        map1.unmap();
        ASSERT_EQ( map1.size(), 0U );
        ASSERT( !map1 );
        ASSERT_EQ( map.cget< char >( 1 ), 'E' );
        ASSERT_EQ( map.cget< char >( 2 ), 'L' );
        ASSERT_EQ( map.cget< char >( 3 ), 'F' );

        ASSERT( map.valid() );

        map.unmap();
        ASSERT_EQ( map.size(), 0U );
        ASSERT( !map );
#endif
    }
};

}
}

#endif // BRICK_MMAP_H
#endif // __unix