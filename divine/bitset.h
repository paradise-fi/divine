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

    // Reference to the underlying graph
    G* m_g; 
    G& g() { return *m_g; }

    std::vector< bool > m_table; // stores visited status of states in bit vector

    /// Dummy function to satisfy API
    int hash( Item ) { return 0; }

    /// Size of underlying storage
    size_t size() const { return m_table.size(); }

    /// Inserts state into the table
    inline Item insert( Item i ) {
        assert( initialized );

        StateId id = g().getStateId( i );
        assert( id >= fromStateId && id <= toStateId );

        m_table[ id - fromStateId ] = true;
        return i;
    }

    /// Inserts state into the table, wrapper to insert
    inline Item insertHinted( Item i, hash_t ) { return insert( i ); }

    /// Reports if requested state has been seen
    bool has( Item i ) {
        assert( initialized );

        StateId id = g().getStateId( i );
        return has( id );
    }

    /// Reports if requested id has been seen
    bool has( StateId id ) {
        assert( id >= fromStateId && id <= toStateId );
        return m_table[ id - fromStateId ];
    }

    /// Returns requested state, marks its state in has (if it was visited)
    inline Item getHinted( Item i, hash_t, bool* has = NULL ) {
        if ( has != NULL ) *has = this->has( i );
        return get( i );
    }

    /// Returns requested state from storage
    Item get( Item i ) {
        assert( initialized );

        StateId id = g().getStateId( i );

        return get( id );
    }

    /// Returns requested state from storage based on its id
    Item get( StateId id ) {
        assert( initialized );

        return g().getState( id );
    }

    /// Sets the size of the underlying table
    void setSize( size_t s ) {
        m_table.resize( s, false );
    }

    /**
     * Which states are to be handled by this bitset:
     * pair: ( from index, to index )
     */
    bool setStates( std::pair< StateId, StateId > states ) {
        fromStateId = states.first;
        toStateId = states.second;
        assert( toStateId || toStateId == fromStateId - 1 );

        if ( toStateId == fromStateId - 1 ) // this peer does not handle any states
            return false; 
        setSize( toStateId - fromStateId + 1 );
        return true;
    }

    /// Reinitializes the table, marks all state as unvisited
    void clear() {
        std::fill( m_table.begin(), m_table.end(), false );
    }

    /**
     * Returns state stored on index off, uninitialized state
     * if this state has not been seen
     */
    Item operator[]( int off ) {
        assert( initialized );
        assert( off >= 0 && off <= toStateId - fromStateId );

        return m_table[ off ] ? g().getState( fromStateId + off ) : Item();
    }

    /// Default constructor
    BitSet() : initialized( false ) {}

    /**
     * Constructs bitset based on given graph (G::Graph is supposed to be
     * generator::Compact) and peerId of peer using this bitset as a table
     */
    BitSet( G *g_, const int peerId, Valid v = Valid(), Equal eq = Equal() )
        : valid( v ), equal( eq ), m_maxsize( -1 ), initialized( true ), m_g( g_ )
    {
        assert( g().initialized );
        assert( peerId >= 0 );
        if ( !setStates( g().getPeerStates( peerId ) ) )
            initialized = false; // no states for this bitset

        // assert that default key is invalid, this is assumed
        // throughout the code
        assert( !valid( Item() ) );
    }
};

}

#endif
