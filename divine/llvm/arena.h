// -*- C++ -*-
#ifndef DIVINE_LLVM_ARENA_H
#define DIVINE_LLVM_ARENA_H

#include <stdint.h>
#include <vector>
#include <map>
#include <divine/blob.h>
#include <divine/pool.h>

namespace divine {
namespace llvm {

/*
 * A small memory allocation arena, with address translation. The "pointers"
 * (represented by the Index type) obtained from and passed to allocate/free
 * need to be "translated" to actual memory address (a real pointer) by calling
 * the translate method. The translated pointer is a short-lived object: never
 * store it. It is invalidated by any memory operations, as well as by an
 * compact/expand cycle.
 */
struct Arena {
    typedef uint32_t word;
    static const size_t BS = 128;
    static const size_t BB = 7;

    struct Index {
        unsigned __align:7;
        bool valid:1;
        unsigned block:8;
        unsigned i:16; /* this is in bytes, so that pointer arithmetic &c. works */

        // to/from a pointer-like object
        operator intptr_t() {
            if ( !valid )
                return 0;
            assert_leq( 1, block );
            return (i | block << BB);
        }

        Index( intptr_t t ) {
            if ( t ) {
                valid = 1;
                i = t & (BS - 1);
                block = t >> BB;
            } else
                valid = 0;
        }

        Index() : __align( 0 ), valid( false ) {}
        Index( int g, int _i ) : __align( 0 ), valid( true ), block( g ), i( _i ) {}

    } __attribute__((packed));

    struct Block {
        enum { Empty, Full };
        int unit; // maybe make 16 bits at some point, but better align for now
        Index free; /* current free cell of this size */
        word data[BS / sizeof(word)];

        word &at( int i, int size ) {
            assert( size % 4 == 0 );
            assert_leq( 1, size );
            return data[ i * (size / 4) ];
        }

        Block() {
            for ( int i = 0; i < BS / sizeof(word); ++i )
                data[ i ] = 0;
        }
    };

    std::vector< Block > blocks;
    std::map< int, int > sizes;

    void newBlock( int size ) {
        assert( size % 4 == 0 );
        blocks.push_back( Block() );
        Block &g = blocks.back();
        g.unit = size;
        int current = sizes[ size ] = blocks.size();
        int slots = BS / size;
        g.free = Index( current, 0 );
        int slot = 0;
        for ( slot = 0; slot < slots - 1; ++slot )
            g.at( slot, size ) = Index( current, (slot + 1) * size );
        g.at( slot, size ) = Index(); // invalid -> end of list
    }

    // FIXME copied from pool
    size_t adjustSize( size_t bytes )
    {
        // round up to a value divisible by 4
        bytes += 3;
        bytes = ~bytes;
        bytes |= 3;
        bytes = ~bytes;
        return bytes;
    }

    word &at( Index i ) {
        assert( i.i % 4 == 0 );
        return block( i.block ).data[ i.i / 4 ];
    }

    Index allocate( int size ) {
        size = adjustSize( size );
        if ( !sizes[size] )
            newBlock( size );
        if ( !block( sizes[size] ).free.valid )
            newBlock( size );
        Block &g = block( sizes[size] );
        assert( g.free.valid );
        Index use = g.free;
        g.free = static_cast< Index >( at( use ) );
        return use;
    }

    void free( Index i ) {
        int size = block( i.block ).unit;
        Block &g = block( sizes[size] );
        at( i ) = g.free;
        g.free = i;
    }

    Block &block( int i ) { // blocks are indexed from 1; 0 is NULL
        assert_leq( 1, i );
        assert_leq( i, blocks.size() );
        return blocks[ i - 1 ];
    }

    void *translate( Index p ) {
        assert( p.valid );
        return block( p.block ).data + (p.i / sizeof(word));
    }

    // Store away the state of the memory arena in a single monolithic memory
    // block. Use "expand" to restore the state of the arena from that point.
    divine::Blob compact( int extra, divine::Pool &p ) {
        divine::Blob b( p, extra + blocks.size() * sizeof( Block ) );
        b.clear();
        for ( int n = 0; n < blocks.size(); ++ n )
            b.get< Block >( extra + ( n * sizeof( Block ) ) ) = blocks[ n ];
        return b;
    }

    void expand( divine::Blob b, int extra ) {
        int count = ( b.size() - extra ) / sizeof( Block );
        assert_eq( ( b.size() - extra ) % sizeof( Block ), 0 );
        blocks.resize( count );
        for ( int n = 0; n < count; ++ n ) {
            blocks[ n ] = b.get< Block >( extra + ( n * sizeof( Block ) ) );
            sizes[ blocks[ n ].unit ] = n + 1;
        }
    }
};

}
}

#endif
