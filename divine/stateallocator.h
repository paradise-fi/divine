// -*- C++ -*- (c) 2007 Petr Rockai <me@mornfall.net>

#include <divine/pool.h>

#ifndef DIVINE_STATEALLOCATOR_H
#define DIVINE_STATEALLOCATOR_H

namespace divine {

struct StateAllocator {
    virtual state_t duplicate_state( const state_t &state ) = 0;
    virtual state_t new_state( std::size_t size ) = 0;
    virtual void delete_state( state_t &st ) = 0;
    virtual void fixup_state( state_t &st ) {}
    virtual ~StateAllocator() {}
};

struct LegacyAllocator : StateAllocator {
    virtual state_t duplicate_state( const state_t &state ) {
        return divine::duplicate_state( state );
    }
    virtual state_t new_state( std::size_t size ) {
        return divine::new_state( size );
    }
    virtual void delete_state( state_t &st ) {
        return divine::delete_state( st );
    }
};

template< typename State >
struct DefaultAllocator : StateAllocator {
    virtual state_t duplicate_state( const state_t &state ) {
        return State( state ).duplicate().state();
    }
    virtual state_t new_state( std::size_t size ) {
        return State::allocate( size ).state();
    }
    virtual void delete_state( state_t &st ) {
        delete[] State( st ).pointer();
    }
};

template< typename State >
struct PooledAllocator : StateAllocator {
    Pool &m_pool;
    PooledAllocator( Pool &p ) : m_pool( p ) {}

    state_t duplicate_state( const state_t &st ) {
        return State( st ).duplicate( m_pool ).state();
    }

    state_t new_state( const std::size_t size ) {
        return State::allocate( m_pool, size ).state();
    }

    void delete_state( state_t &st ) {
        State( st ).deallocate( m_pool );
    }
};
}

#endif
