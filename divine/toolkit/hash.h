// -*- C++ -*- (c) Vladimír Štill <xstill@fi.muni.cz>
#include <cstdint>
#include <divine/toolkit/jenkins.h>

#ifndef DIVINE_TOOLKIT_HASH_H
#define DIVINE_TOOLKIT_HASH_H

namespace divine {

typedef uint64_t hash64_t;
typedef std::pair< hash64_t, hash64_t > hash128_t;

static hash128_t spookyHash( const void *message, size_t length, uint64_t seed1, uint64_t seed2 ) {
    return jenkins::SpookyHash::Hash128( message, length, seed1, seed2 );
}

struct SpookyState {

    SpookyState( uint64_t seed1, uint64_t seed2 ) : state() {
        state.Init( seed1, seed2 );
    }
    SpookyState() = delete;
    SpookyState( const SpookyState & ) = delete;
    SpookyState &operator=( const SpookyState & ) = delete;

    void update( const void *message, size_t length ) {
        state.Update( message, length );
    }

    hash128_t finalize() {
        return state.Final();
    }

  private:
    jenkins::SpookyHash state;
};

}

#endif // DIVINE_TOOLKIT_HASH_H

