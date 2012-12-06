#include <algorithm>
#include <limits>

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

    char* back() {
        assert( !v.empty() );
        return &v[ v.size() - block ];
    }

    const char* back() const {
        assert( !v.empty() );
        return &v[ v.size() - block ];
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

/*
Structure for storing generated successors.
The top-most successor (obtainable with back()) is typically used to construct the new state and if everything succeeds,
push_back() is called, which creates new block of memory to work on.
Each batch of sucessors has to be preceded by a call to setInfo, which can be used to set priorities and custom information.
All priorities except the first one (= process priorities) are sorted before comparison, the edge/channel priority is always
compared first.
*/
template < typename EdgeInfo  >
class SuccessorList {
    struct Info {
        unsigned int index;
        EdgeInfo edge;
        std::vector< int > prio;

        Info( unsigned int _index, EdgeInfo _edge )
            : index( _index ), edge( _edge ), prio() {}

        Info( unsigned int _index, EdgeInfo _edge, const std::initializer_list< int >& list )
            : index( _index ), edge( _edge ) , prio( list ) {}

        bool equalPrio( const std::vector< int >& max ) const {
            for ( unsigned int i = 0; i < max.size(); i++ ) {
                assert( i < prio.size() );
                if ( prio[ i ] < max[ i ] )
                    return false;
                assert( max[ i ] == prio[ i ] );
                assert( i <= 1 || max[ i-1 ] >= max[ i ] );
            }
            return true;
        }

        void updateMaxPrio( std::vector< int >& max ) {
            // leave the first element, sort the rest
            assert( prio.size() > 1 );
            std::sort( prio.begin() + 1, prio.end(), [] ( int a, int b ) { return a > b; } );
            unsigned int end = std::min( prio.size(), max.size() );
            // update maximum
            for ( unsigned int i = 0; i < end; i++ ) {
                if ( prio[ i ] < max[ i ] )     // lower than max priority - ignore
                    return;
                if ( prio[ i ] > max[ i ] ) {   // higher than max priority - set the new maximum
                    max = prio;
                    return;
                }
            }
            // equal to max priority - truncate max if necessary
            max.resize( end );
        }
    };

    BlockList bl;
    std::vector< int > maxPrio;
    std::vector< Info > info;

public:
    // Takes the size of each state and maximum number of priorities (0 disables priorities)
    SuccessorList( unsigned int stateSize, bool useProrities ) : bl( stateSize, 1 ), maxPrio() {
        if ( useProrities )
            maxPrio.push_back( std::numeric_limits< int >::min() );
    }

    // Set custom information, edge priority and priority of the first process
    void setInfo( EdgeInfo edge, int chanPrio, int firstPrio ) {
        assert( !bl.empty() );
        info.push_back( Info( bl.size()-1, edge, { chanPrio, firstPrio } ) );
    }

    // Set priority of additional processes
    void appendPriority( int p ) {
        assert( !info.empty() );
        info.back().prio.push_back( p );
    }

    // Returns cutom information from the last setInfo call
    EdgeInfo getLastEdge() {
        assert( !info.empty() );
        return info.back().edge;
    }

    void push_back() {
        bl.push_back();
    }

    // Returns size including the top-most state
    unsigned int size() const {
        return bl.size();
    }

    // Returns size excluding the top-most state
    unsigned int count() const {
        assert( !bl.empty() );
        return bl.size() - 1;
    }

    char *back() {
        return bl.back();
    }

    char *operator[]( unsigned int i ) {
        return bl[ i ];
    }

    // Sorts all priorities and computes the highest priority.
    // Priorities do not work properly if this is not called.
    // Sucessors added after this call are considered to have priority aqual to the highest pririty
    void finalizePriorities() {
        if ( !maxPrio.empty() ) {
            for ( auto i = info.begin(); i != info.end(); ++i ) {
                assert( i->prio.size() >= 2 );
                i->updateMaxPrio( maxPrio );
            }
        }
        info.push_back( Info( bl.size()-1, NULL ) );
    }

    // Calls custom callback for all successors (except the top-most one) that have the highest priority
    // Second argument can be used to skip given number of stored successors
    template < typename Func >
    void for_each( Func callback, unsigned int begin = 0 ) {
        unsigned int s = begin;
        for ( auto inf = info.begin(); inf+1 < info.end(); ++inf ) {
            unsigned int end = (inf+1)->index;
            if ( s < end && inf->equalPrio( maxPrio ) ) {
                for ( ; s < end; ++s ) {
                    callback( bl[ s ], inf->edge );
                }
            } else
                s = end;
        }
        unsigned int end = bl.size()-1;
        for ( ; s < end; ++s )
            callback( bl[ s ], NULL );
    }
};
