// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <divine/config.h>
#include <divine/report.h>
#include <divine/blob.h>
#include <divine/hashset.h>
#include <divine/bitset.h>
#include <divine/visitor.h>
#include <wibble/sfinae.h>

#ifndef DIVINE_ALGORITHM_H
#define DIVINE_ALGORITHM_H

namespace divine {
namespace algorithm {

inline int workerCount( Config *c ) {
    if ( !c )
        return 1;
    return c->workers;
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
    // setting slack to negative will cause all states considered equal (needed for hash compaction)
    inline hash_t operator()( Blob a, Blob b ) const {
        assert( a.valid() );
        assert( b.valid() );
        return ( slack < 0 ) || ( a.compare( b, slack, std::max( a.size(), b.size() ) ) == 0 );
    }
};

struct Algorithm
{
    typedef divine::Config Config;

    Config *m_config;
    int m_slack;
    Hasher hasher;
    Equal equal;
    Result m_result;

    int *m_initialTable, m_initialTable_;

    Result &result() { return m_result; }

    bool want_ce;

    Config &config() {
        assert( m_config );
        return *m_config;
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

    /// Sets the offset for generator part of state data
    template < typename G >
    void setSlack( G *g ) {
        int real = g->setSlack( m_slack );
        hasher.setSlack( real );
        equal.setSlack( real );
    }

    /**
     * Initializes the graph generator by reading a file, used
     * in parallel/distributed environment on master peer.
     */
    template< typename G, typename D >
    void initGraph( G *g, D *domain ) {
        assert( g );
        assert( domain );
        assert( m_config ); // this is the master instance
        g->setDomainSize( domain->mpi.rank(), domain->mpi.size(), domain->peers() );
        initGraph( g );
    }

    /// Initializes the graph generator by reading a file
    template< typename G >
    void initGraph( G *g ) {
        assert( g );
        if ( m_config ) { // this is the master instance
            setSlack( g );
            g->read( m_config->input );
            if ( !m_config->property.empty() )
                g->useProperty( m_config->property );
        }
    }

    Algorithm( Config *c = 0, int slack = 0 )
        : m_config( c ), m_slack( slack )
    {
        m_initialTable_ = 4096;
        m_initialTable = &m_initialTable_;
        if ( c ) {
            want_ce = c->wantCe;
        }
    }
};

template< typename G >
struct AlgorithmUtilsCommon : public virtual Algorithm {

    typedef typename G::Graph::Table Table;

    G* m_g;
    Table *m_table;
    int peerId;

    template< typename D >
    void init( D *domain , int commsParam = 0 ) {
        initGraph( m_g, domain );
        domain->comms.setup( commsParam );
    }

    void init() { initGraph( m_g ); }

    /**
     * Initializes algorithm with used generator, a size of the table and peer id.
     * Size of the table and peerId need to be specified only outside of the master peer.
     */
    void initPeer( G* g, int* initialTable = NULL, int peerId = -1 ) {
        assert( g );
        if ( initialTable != NULL )
            m_initialTable = initialTable;
        m_g = g;
        this->peerId = peerId;
        setSlack( g );
    }

    Table &table() {
        if ( !this->m_table )
            this->m_table = makeTable();
        return *this->m_table;
    }

    virtual Table* makeTable() = 0;

    AlgorithmUtilsCommon() : m_table( NULL ) {}

    ~AlgorithmUtilsCommon() {
        safe_delete( m_table );
    }
};

/// HashSet specialization: HashSet is used as a storage of visited states
template< typename G, typename DefaultTable = typename G::Graph::Table >
struct AlgorithmUtils : public AlgorithmUtilsCommon< G >{

    typedef typename G::Graph::Table Table;

    Table *makeTable() {
        return new Table( this->hasher, divine::valid< typename G::Node >(),
                          this->equal, *this->m_initialTable );
    }
};

#define BITSET BitSet< typename G::Graph, divine::valid< typename G::Node >, algorithm::Equal >

/**
 * BitSet specialization: BitSet with combination of generator::Compact
 * is used as a storage of visited states.
 */
template< typename G >
struct AlgorithmUtils< G, typename wibble::EnableIf<
    wibble::TSame< typename G::Graph::Table, BITSET >, BITSET >::T >
    : AlgorithmUtilsCommon< G > {

    typedef typename G::Graph::Table Table;

    Table *makeTable() {
        assert( this->peerId >= 0 );
        return new Table( &this->m_g->base(), this->peerId,
                          divine::valid< typename G::Node >(), this->equal );
    }
};

}
}

#endif
