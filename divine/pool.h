// -*- C++ -*-
#include <cassert>
#include <map>
#include <iostream>

#ifndef DIVINE_POOL_H
#define DIVINE_POOL_H

namespace divine {

struct Pool {

    struct Block
    {
        size_t size;
        char *start;
        Block() : size( 0 ), start( 0 ) {}
    };

    struct Group
    {
        size_t item, used, total, peak, freed, allocated, stolen;
        char **free; // reuse allocation
        char *current; // tail allocation
        char *last; // bumper
        std::vector< Block > blocks;
        Group()
            : item( 0 ), used( 0 ), total( 0 ), peak( 0 ),
              freed( 0 ), allocated( 0 ), stolen( 0 ),
              free( 0 ), current( 0 ), last( 0 )
        {}
    };

    typedef std::vector< Group > Groups;
    Groups m_groups;

    Pool()
    {
    }

    size_t peakAllocation() {
        size_t total = 0;
        Groups::iterator i;
        for ( i = m_groups.begin(); i != m_groups.end(); ++i ) {
            total += i->total;
        }
        return total;
    }

    size_t peakUsage() {
        size_t total = 0;
        Groups::iterator i;
        for ( i = m_groups.begin(); i != m_groups.end(); ++i ) {
            total += i->peak;
        }
        return total;
    }

    void newBlock( Group *g )
    {
        Block b;
        size_t s = std::min( 2 * g->blocks.back().size, size_t( 1024 * 1024 ) );
        b.size = s;
        b.start = new char[ s ];
        g->current = b.start;
        g->last = b.start + s;
        g->total += s;
        g->blocks.push_back( b );
    }

    size_t adjustSize( size_t bytes )
    {
        // round up to a value divisible by 4
        bytes += 3;
        bytes = ~bytes;
        bytes |= 3;
        bytes = ~bytes;
        return bytes;
    }

    Group *group( size_t bytes )
    {
        if ( bytes / 4 >= m_groups.size() )
            return 0;
        assert( bytes % 4 == 0 );
        if ( m_groups[ bytes / 4 ].total )
            return &(m_groups[ bytes / 4 ]);
        else
            return 0;
    }

    Group *createGroup( size_t bytes )
    {
        assert( bytes % 4 == 0 );
        Group g;
        g.item = bytes;
        Block b;
        b.size = 1024 * bytes;
        b.start = new char[ b.size ];
        g.blocks.push_back( b );
        g.current = b.start;
        g.last = b.start + b.size;
        g.total = b.size;
        m_groups.resize( std::max( bytes / 4 + 1, m_groups.size() ), Group() );
        assert( m_groups[ bytes / 4 ].total == 0 );
        m_groups[ bytes / 4 ] = g;
        assert( g.total != 0 );
        assert( group( bytes ) );
        assert( group( bytes )->total == g.total );
        // m_groups.insert( std::make_pair( bytes, g ) );
        return group( bytes );
    }

    char *alloc( size_t bytes )
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
        g->used += bytes;
        g->allocated += bytes;
        if ( g->used > g->peak ) g->peak = g->used;
        assert( g->used <= g->total );

        return ret;
    }

    size_t pointerSize( char *_ptr )
    {
        return 0; // replace with magic
    }

    void release( Group *g, char *ptr, size_t size )
    {
        assert( g );
        assert( g->item == size );
        g->freed += size;
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
        assert( g->used >= size );
        g->used -= size;
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
        g->total += size;
        g->stolen += size;
        release( g, ptr, size );
    }

    std::ostream &printStatistics( std::ostream &s ) {
        for ( Groups::iterator i = m_groups.begin(); i != m_groups.end(); ++i ) {
            if ( i->total == 0 )
                continue;
            s << "group " << i->item
              << " holds " << i->used
              << " (peaked " << i->peak
              << "), allocated " << i->allocated
              << " and freed " << i->freed << " bytes in "
              << i->blocks.size() << " blocks"
              << std::endl;
        }
        return s;
    };
};

}

namespace std {

inline void swap( divine::Pool &a, divine::Pool &b )
{
    swap( a.m_groups, b.m_groups );
}

}

#endif
