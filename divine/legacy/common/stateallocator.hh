// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>
#ifndef DIVINE_LEGACY_STATEALLOCATOR_HH
#define DIVINE_LEGACY_STATEALLOCATOR_HH

#include <divine/stateallocator.h>

namespace divine {

struct LegacyStateAllocator {
    StateAllocator *m_allocator;

    LegacyStateAllocator() : m_allocator( 0 ) {}

    void setAllocator( StateAllocator *a ) {
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

    void delete_state( state_t &st ) {
        assert_die();
    }
};

}

#endif
