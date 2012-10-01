// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>
#ifndef DIVINE_LEGACY_STATEALLOCATOR_HH
#define DIVINE_LEGACY_STATEALLOCATOR_HH

#include <divine/graph/allocator.h>

namespace divine {

struct IndirectLegacyAllocator {
    LegacyAllocator *m_allocator;

    IndirectLegacyAllocator() : m_allocator( 0 ) {}

    void setAllocator( LegacyAllocator *a ) {
        m_allocator = a;
    }

    state_t duplicate_state( const state_t &state ) {
        assert( m_allocator );
        return m_allocator->duplicate_state( state );
    }

    state_t new_state( std::size_t size ) {
        assert( m_allocator );
        return m_allocator->new_state( size );
    }

    void delete_state( state_t & ) {
        assert_die();
    }
};

}

#endif
