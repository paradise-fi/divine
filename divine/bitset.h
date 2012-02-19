// -*- C++ -*- (c) 2011 Jiri Appl <jiri@appl.name>

#include "divine/compactstate.h"

#ifndef DIVINE_BITSET_H
#define DIVINE_BITSET_H

namespace divine {

/**
 * Implements API equivalent of divine::HashSet and can be used as a storage
 * of visited states. It requires a close cooperation with selected generator.
 * It can be used in cooperation with generator::Compact to achieve constant time
 * access to states.
 */
template< typename G,
    typename _Valid = divine::valid< typename G::Node >,
    typename _Equal = divine::equal< typename G::Node > >
struct BitSet
{
    typedef typename G::Node Item;
    typedef _Valid Valid;
    typedef _Equal Equal;

    Valid valid;
    Equal equal;
    size_t m_maxsize;

    StateId fromStateId, toStateId; // indices of boundary states to be stored
    bool initialized; // has it been initialized by a graph
    unsigned visited; // number of states which were inserted (marked as visited)
    StateId lowest; // lowest id of a visited state

    // Reference to the underlying graph
    G* m_g; 
    G& g() { return *m_g; }

#define USE_BOOL_VECTOR

#ifndef USE_BOOL_VECTOR
//#define UBSIZE ( 8 * sizeof( unsigned ) )
#define UBSIZE 32
#define UBINDEX ( ( id - fromStateId ) / UBSIZE )
#define UBBIT ( 1 << ( ( id - fromStateId ) % UBSIZE ) )
#endif

#ifdef USE_BOOL_VECTOR
    std::vector< bool > m_table; // stores visited status of states in bit vector
#else
    std::vector< uint32_t > m_table;
    unsigned m_table_size;
#endif

    std::map< unsigned, unsigned > sliceCount;

    /// Dummy function to satisfy API
    int hash( Item ) { return 0; }

    /// Size of underlying storage
    size_t size() const {
#ifdef USE_BOOL_VECTOR
        return m_table.size();
#else
        return m_table_size;
#endif
    }

    /// Inserts state into the table
    inline Item insert( Item i ) {
        assert( initialized );

        const StateId id = g().getStateId( i );
        assert( id >= fromStateId && id <= toStateId );

        if ( !has( id ) ) {
            visited++;
            if ( id < lowest )
                lowest = id;
        }

#ifdef USE_BOOL_VECTOR
        m_table[ id - fromStateId ] = true;
#else
        m_table[ UBINDEX ] |= UBBIT;
#endif
        return i;
    }

    inline Item insert( Item i, const unsigned sliceId ) {
        if ( !has( i ) ) sliceCount[ sliceId ]++;
        return insert( i );
    }

    /// Inserts state into the table, wrapper to insert
    inline Item insertHinted( Item i, hash_t ) { return insert( i ); }

    /// Reports if requested state has been seen
    bool has( Item i ) {
        assert( initialized );

        const StateId id = g().getStateId( i );
        return has( id );
    }

    /// Reports if requested id has been seen
    inline bool has( StateId id ) {
        assert( id >= fromStateId && id <= toStateId );
#ifdef USE_BOOL_VECTOR
        return m_table[ id - fromStateId ];
#else
        return m_table[ UBINDEX ] & UBBIT;
#endif
    }

    /// Returns requested state, marks its state in has (if it was visited)
    inline Item getHinted( Item i, hash_t, bool* has = NULL ) {
        if ( has != NULL ) *has = this->has( i );
        return get( i );
    }

    /// Returns requested state from storage
    Item get( Item i ) {
        assert( initialized );

        const StateId id = g().getStateId( i );

        return get( id );
    }

    /// Returns requested state from storage based on its id
    Item get( StateId id ) {
        assert( initialized );

        return g().getState( id );
    }

    /// Removes visited status of state with id id
    bool remove( StateId id ) {
        assert( initialized );

        if ( !has( id ) ) return false;

        visited--;
#ifdef USE_BOOL_VECTOR
        m_table[ id - fromStateId ] = false;
#else
        m_table[ UBINDEX ] &= ~UBBIT;
#endif
        if ( lowest == id ) {
            if ( visited ) {
#ifdef USE_BOOL_VECTOR
                for ( ; lowest - fromStateId < m_table.size() && !has( lowest ); lowest++ );
// TODO following appeared to be slower
//                 std::vector< bool >::const_iterator it = m_table.begin() + lowest - fromStateId + 1;
//                 for ( ; it != m_table.end() && !*it; ++it );
//                 lowest = ( it == m_table.end() ? m_table.size() : *it ) + fromStateId;
#else
                unsigned i = UBINDEX;
                for ( ; i < m_table.size() && !m_table[ i ]; i++ );
                if ( i >= m_table.size() )
                    lowest = m_table.size() + fromStateId;
                else {
                    assert( m_table[ i ] );
                    const uint32_t v = m_table[ i ];
                    static const int MultiplyDeBruijnBitPosition[32] =
                    {
                        0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
                        31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
                    };
                    lowest = fromStateId + UBSIZE * i + MultiplyDeBruijnBitPosition[((uint32_t)((v & -v) * 0x077CB531U)) >> 27];
                }
#endif
            } else
                lowest = m_table.size() + fromStateId;
        }
        return true;
    }

    /// Removes visited status of state i
    bool remove( Item i ) {
        assert( initialized );

        StateId id = g().getStateId( i );

        return remove( id );
    }

    bool remove( Item i, const unsigned sliceId ) {
        if ( has( i ) ) {
            assert( sliceCount[ sliceId ] > 0 );
            if ( !( --sliceCount[ sliceId ] ) )
                sliceCount.erase( sliceId ); //xxx
        }
        return remove( i );
    }

    /// Sets the size of the underlying table
    void setSize( size_t s ) {
#ifdef USE_BOOL_VECTOR
        m_table.resize( s, false );
#else
        m_table.resize( s / UBSIZE + 1, 0 );
        m_table_size = s;
#endif
        lowest = fromStateId + s;
    }

    /**
     * Which states are to be handled by this bitset:
     * pair: ( from index, to index )
     */
    bool setStates( std::pair< StateId, StateId > states ) {
        fromStateId = states.first;
        toStateId = states.second;
        assert( toStateId || toStateId == fromStateId - 1 );

        if ( toStateId == fromStateId - 1 ) { // this peer does not handle any states
            setSize( 0 );
            return false;
        } else {
            setSize( toStateId - fromStateId + 1 );
            return true;
        }
    }

    /// Reinitializes the table, marks all state as unvisited
    void clear() {
#ifdef USE_BOOL_VECTOR
        std::fill( m_table.begin(), m_table.end(), false );
#else
        std::fill( m_table.begin(), m_table.end(), 0 );
#endif
        visited = 0;
        sliceCount.clear();
        lowest = m_table.size() + fromStateId;
    }

    /// Makes all states visited
    void visitAll() {
#ifdef USE_BOOL_VECTOR
        std::fill( m_table.begin(), m_table.end(), true );
#else
        std::fill( m_table.begin(), m_table.end(), ~0 );
#endif
        visited = m_table.size();
        sliceCount[ 0 ] = visited;//
        lowest = fromStateId;
    }

    /**
     * Returns state stored on index off, uninitialized state
     * if this state has not been seen
     */
    Item operator[]( int off ) {
        assert( initialized );
        assert( off >= 0 && off <= toStateId - fromStateId );

#ifdef USE_BOOL_VECTOR
        return m_table[ off ] ? g().getState( fromStateId + off ) : Item();
#else
        return ( m_table[ off / UBSIZE ] & ( 1 << ( off % UBSIZE ) ) ) ? g().getState( fromStateId + off ) : Item();
#endif
    }

    /// Returns a number of visited states
    unsigned statesCount() { return visited; }

    /// Returns an id of a visited state with the lowest id
    StateId lowestState() { return lowest; }

    /// Default constructor
    BitSet() : initialized( false ) {}

    /**
     * Constructs bitset based on given graph (G::Graph is supposed to be
     * generator::Compact) and peerId of peer using this bitset as a table
     */
    BitSet( G *g_, const int peerId, Valid v = Valid(), Equal eq = Equal() )
        : valid( v ), equal( eq ), m_maxsize( -1 ), initialized( false ), visited( 0 ), m_g( g_ )
    {
        assert( g().initialized );
        assert( peerId >= 0 );
        initialized = setStates( g().getPeerStates( peerId ) ); // TODO this might need to be set to true eitherway

        // assert that default key is invalid, this is assumed
        // throughout the code
        assert( !valid( Item() ) );
    }
};

}

#endif
