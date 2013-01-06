// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>
#include <wibble/test.h> // for assert
#include <vector>
#include <iostream>

#ifndef DIVINE_POOL_H
#define DIVINE_POOL_H

namespace divine {

struct FakePool {

    FakePool() {}
    FakePool( const FakePool & ) {}
    ~FakePool() {}

    char *allocate( size_t s ) {
        return new char[ s ];
    }

    template< typename T >
    void deallocate( T *_ptr, size_t size = 0 )
    {
        return steal( _ptr, size );
    }

    template< typename T >
    void free( T *_ptr, size_t size = 0 )
    {
        delete[] static_cast< char * >( _ptr );
    }

    template< typename T >
    void steal( T *_ptr, size_t size = 0 ) {
        delete[] static_cast< char * >( _ptr );
    }
};

#ifndef O_POOLS

typedef FakePool Pool;

#else

struct Pool {
    struct Block
    {
        size_t size;
        char *start;
        Block() : size( 0 ), start( 0 ) {}
    };

    struct Group
    {
        int item;
        int used, total; /* XXX: Deprecated, but part of CESMI ABI. */
        char **free; // reuse allocation
        char *current; // tail allocation
        char *last; // bumper
        std::vector< Block > blocks;
        Group()
            : item( 0 ), used( 0 ), total( 0 ),
              free( 0 ), current( 0 ), last( 0 )
        {}
    };

    // for FFI
    int m_groupCount;
    int m_groupStructSize;
    Group *m_groupArray;

    typedef std::vector< Group > Groups;
    Groups m_groups;

    Pool();
    Pool( const Pool & );
    ~Pool();

    // ignore assignments
    Pool &operator=( const Pool & ) { return *this; }

    void newBlock( Group *g )
    {
        Block b;
        int s = std::min( 4 * int( g->blocks.back().size ),
                          4 * 1024 * 1024 );
        b.size = s;
        b.start = new char[ s ];
        g->current = b.start;
        g->last = b.start + s;
        g->blocks.push_back( b );
    }

    int adjustSize( int bytes )
    {
        // round up to a value divisible by 4
        bytes += 3;
        bytes = ~bytes;
        bytes |= 3;
        bytes = ~bytes;
        return bytes;
    }

    Group *group( int bytes )
    {
        if ( bytes / 4 >= int( m_groups.size() ) )
            return 0;
        assert_eq( bytes % 4, 0 );
        if ( m_groups[ bytes / 4 ].total )
            return &(m_groups[ bytes / 4 ]);
        else
            return 0;
    }

    void ffi_update() {
        m_groupCount = m_groups.size();
        m_groupArray = &m_groups.front();
    }

    Group *createGroup( int bytes )
    {
        assert( bytes % 4 == 0 );
        Group g;
        g.item = bytes;
        Block b;
        b.size = std::min( std::max( 1024 * 1024, bytes ), 4096 * bytes );
        b.start = new char[ b.size ];
        g.blocks.push_back( b );
        g.current = b.start;
        g.last = b.start + b.size;
        g.total = 1;
        m_groups.resize( std::max( bytes / 4 + 1, int( m_groups.size() ) ), Group() );

        ffi_update();

        assert_eq( m_groups[ bytes / 4 ].total, 0 );
        m_groups[ bytes / 4 ] = g;
        assert_neq( g.total, 0 );
        assert( group( bytes ) );
        return group( bytes );
    }

    char *allocate( size_t bytes )
    {
        assert( bytes );
        bytes = adjustSize( bytes );
        char *ret = 0;

        Group *g = group( bytes );
        if (!g)
            g = createGroup( bytes );

        if ( g->free ) {
            ret = reinterpret_cast< char * >( g->free );
            g->free = *reinterpret_cast< char *** >( ret );
        } else {
            if ( g->current + bytes > g->last ) {
                newBlock( g );
            }
            ret = g->current;
            g->current += bytes;
        }

        return ret;
    }

    size_t pointerSize( char * )
    {
        assert_die();
        return 0; // replace with magic
    }

    void release( Group *g, char *ptr, int size )
    {
        assert( g );
        assert_eq( g->item, size );
        char ***ptr3 = reinterpret_cast< char *** >( ptr );
        char **ptr2 = reinterpret_cast< char ** >( ptr );
        *ptr3 = g->free;
        g->free = ptr2;
    }

    template< typename T >
    void free( T *_ptr, size_t size = 0 )
    {   // O(1) free
        if ( !_ptr ) return; // noop
        char *ptr = reinterpret_cast< char * >( _ptr );
        if ( !size )
            size = pointerSize( ptr );
        else {
            assert( size );
            size = adjustSize( size );
        }
        assert( size );
        Group *g = group( size );
        release( g, ptr, size );
    }

    template< typename T >
    void steal( T *_ptr, size_t size = 0 )
    {
        char *ptr = reinterpret_cast< char * >( _ptr );
        if ( !size )
            size = pointerSize( ptr );
        else {
            assert( size );
            size = adjustSize( size );
        }
        assert( size );
        Group *g = group( size );
        if ( !g )
            g = createGroup( size );
        assert( g );
        release( g, ptr, size );
    }

    template< typename T >
    void deallocate( T *_ptr, size_t size = 0 )
    {
        return steal( _ptr, size );
    }
};

#endif

std::ostream &operator<<( std::ostream &o, const Pool &p );

}

namespace std {

template<>
inline void swap< divine::Pool >( divine::Pool &a, divine::Pool &b )
{
#ifdef O_POOLS
    swap( a.m_groups, b.m_groups );
#endif
}

}

#endif
