// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <divine/config.h>
#include <divine/report.h>
#include <divine/blob.h>
#include <divine/hashmap.h>
#include <divine/visitor.h>

#ifndef DIVINE_ALGORITHM_H
#define DIVINE_ALGORITHM_H

namespace divine {
namespace algorithm {

inline int workerCount( Config *c ) {
    if ( !c )
        return 1;
    return c->workers();
}

struct Hasher {
    int slack;

    Hasher( int s = 0 ) : slack( s ) {}
    void setSlack( int s ) { slack = s; }

    inline hash_t operator()( Blob b ) const {
        assert( b.valid() );
        return b.hash( slack, b.size() );
    }
};

struct Equal {
    int slack;
    Equal( int s = 0 ) : slack( s ) {}
    void setSlack( int s ) { slack = s; }
    inline hash_t operator()( Blob a, Blob b ) const {
        assert( a.valid() );
        assert( b.valid() );
        return a.compare( b, slack, std::max( a.size(), b.size() ) ) == 0;
    }
};

struct Algorithm
{
    typedef Blob Node; // Umm.

    typedef HashMap< Node, Unit, Hasher,
                     divine::valid< Node >, Equal > Table;

    Config *m_config;
    int m_slack;
    Hasher hasher;
    Equal equal;
    Table *m_table;
    Result m_result;

    int *m_initialTable, m_initialTable_;

    Result &result() { return m_result; }

    bool want_ce;

    Config &config() {
        assert( m_config );
        return *m_config;
    }

    Table &table() {
        if ( !m_table )
            m_table = new Table( hasher, divine::valid< Node >(), equal,
                                 *m_initialTable );
        return *m_table;
    }

    std::ostream &progress() {
        return Output::output().progress();
    }

    void livenessBanner( bool valid ) {
        progress() << " ===================================== " << std::endl
                   << ( valid ?
                      "       Accepting cycle NOT found       " :
                      "         Accepting cycle FOUND         " )
                   << std::endl
                   << " ===================================== " << std::endl;
    }

    void safetyBanner( bool valid ) {
        progress() << " ===================================== " << std::endl
                   << ( valid ?
                      "          Goal state NOT found         " :
                      "            Goal state FOUND           " )
                   << std::endl
                   << " ===================================== " << std::endl;
    }

    template< typename G >
    void initGraph( G &g ) {
        int real = g.setSlack( m_slack );
        hasher.setSlack( real );
        equal.setSlack( real );
        if ( m_config ) { // this is the master instance
            g.read( m_config->input() );
        }
    }

    Algorithm( Config *c = 0, int slack = 0 )
        : m_config( c ), m_slack( slack ), m_table( 0 )
    {
        m_initialTable_ = 4096;
        m_initialTable = &m_initialTable_;
        if ( c ) {
            want_ce = c->generateCounterexample();
        }
    }

    ~Algorithm() {
        safe_delete( m_table );
    }
};

}
}

#endif
