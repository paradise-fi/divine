// -*- C++ -*-
#include <algorithm>
#include <limits>
#include <wibble/test.h>

namespace divine {
namespace timed {

// Simple structure for storing blocks of data with eqaual size
class BlockList {
    const unsigned int block;
    std::vector< char > v;

public:
    explicit BlockList( unsigned int blockSize ) : block( blockSize ) {}

    BlockList( unsigned int blockSize, unsigned int size ) : block( blockSize ), v( size*blockSize ) {}

    void push_back() {
        v.resize( v.size() + block );
    }

    void push_back( const void* ptr ) {
        push_back();
        memcpy( back(), ptr, block );
    }

    void duplicate_back() {
        push_back();
        memcpy( back(), back() - block, block );
    }

    char* back() {
        assert( !v.empty() );
        return &*v.end() - block;
    }

    const char* back() const {
        assert( !v.empty() );
        return &*v.end() - block;
    }

    void assign_back( const void* ptr ) {
        memcpy( back(), ptr, block );
    }

    void pop_back() {
        assert( !v.empty() );
        v.resize( v.size() - block );
    }

    unsigned int size() const {
        return v.size() / block;
    }

    bool empty() const {
        return v.empty();
    }

    void resize( unsigned int size ) {
        v.resize( size*block );
    }

    char* operator[]( unsigned int i ) {
        assert( i*block < v.size() );
        return &v[ i*block ];
    }

    const char* operator[]( unsigned int i ) const {
        assert( i*block < v.size() );
        return &v[ i*block ];
    }
};

// Priority valuation. The first value is the most significant, other values have equal valuation.
class PrioVal {
    std::vector< int > v;

public:
	// With b = true, the lowest priority is constructed
	// With b = false, the priority that is equal to any other is constructed
    explicit PrioVal( bool b ) : v() {
        if ( b )
            v.push_back( std::numeric_limits< int >::min() );
    }

	// Costructs a priority by specifying first two values
    PrioVal( int p1, int p2 ) : v({ p1, p2 }) {}

	// Append additional values
    void append( int p ) {
        v.push_back( p );
    }

	// Finalize a priority valuation by sorting all values except the first one
    void finalize() {
        // leave the first element, sort the rest
        assert( v.size() > 1 );
        std::sort( v.begin() + 1, v.end(), [] ( int a, int b ) { return a > b; } );
    }

	// Updates max if it is lower than current priority
    bool updateMax( PrioVal& max ) const {
        unsigned int size = std::min( v.size(), max.v.size() );
        for ( unsigned int i = 0; i < size; i++ ) {
            assert( i <= 1 || v[ i-1 ]>= v[ i ] );
            if ( v[ i ] < max.v[ i ] )     // lower than max priority - ignore
                return false;
            if ( v[ i ] > max.v[ i ] ) {   // higher than max priority - set the new maximum
                max.v = v;
                return true;
            }
        }
        // equal to max priority - truncate max if necessary
        max.v.resize( size );
        return true;
    }

	// Check for equality
    bool equal( const PrioVal& max ) const {
        for ( unsigned int i = 0; i < max.v.size(); i++ ) {
            assert( i < v.size() );
            if ( v[ i ] < max.v[ i ] )
                return false;
            assert( max.v[ i ] == v[ i ] );
            assert( i <= 1 || max.v[ i-1 ] >= max.v[ i ] );
        }
        return true;
    }
};

// Structure holding information about enabled state (priority, custom information about edges)
template < typename EdgeInfo >
struct EnabledInfo {
    PrioVal prio;
    std::vector< EdgeInfo > edges;
    bool valid;

    EnabledInfo( EdgeInfo edge, int chanPrio, int edgePrio ) : prio( chanPrio, edgePrio ), edges({ edge }), valid( true ) {}

    void addEdge( EdgeInfo edge, int edgePrio ) {
        edges.push_back( edge );
        prio.append( edgePrio );
    }
};

}
}
