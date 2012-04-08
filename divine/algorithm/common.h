// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <divine/meta.h>
#include <divine/blob.h>
#include <divine/hashset.h>
#include <divine/bitset.h>
#include <divine/visitor.h>
#include <divine/parallel.h>
#include <wibble/sfinae.h>

#ifndef DIVINE_ALGORITHM_H
#define DIVINE_ALGORITHM_H

namespace divine {
namespace algorithm {

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
    bool allEqual;
    Equal( int s = 0 ) : slack( s ), allEqual( false ) {}
    void setSlack( int s ) { slack = s; }

    inline hash_t operator()( Blob a, Blob b ) const {
        assert( a.valid() );
        assert( b.valid() );
        return allEqual || ( a.compare( b, slack, std::max( a.size(), b.size() ) ) == 0 );
    }
};

struct Algorithm
{
    typedef divine::Meta Meta;

    Meta m_meta;
    int m_slack;
    Hasher hasher;
    Equal equal;

    meta::Result &result() { return meta().result; }

    virtual void run() = 0;
    virtual MpiBase *mpi() { return NULL; }

    bool want_ce;

    Meta &meta() {
        return m_meta;
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

    /// Initializes the graph generator by reading a file
    template< typename G >
    void initGraph( G *g ) {
        assert( g );
        setSlack( g );
        g->read( meta().input.model );
        g->useProperty( meta().input );
        g->setDomainSize( meta().execution.thisNode,
                          meta().execution.nodes,
                          meta().execution.nodes * meta().execution.threads );
    }

    Algorithm( Meta m, int slack = 0 )
        : m_meta( m ), m_slack( slack )
    {
        want_ce = meta().output.wantCe;
    }
};

template< typename G >
struct AlgorithmUtilsCommon : public virtual Algorithm
{
    typedef typename G::Table Table;

    G *m_graph;
    DomainWorkerBase *m_domainWorker;

    Table *m_table;

    void init( G *g, DomainWorkerBase *dwb) {
        m_graph = g;
        this->initGraph( g );
        m_domainWorker = dwb;
    }

    G &graph() {
        return *( G *) this->m_graph;
    }

    Table &table() {
        if ( !this->m_table )
            this->m_table = makeTable();
        return *this->m_table;
    }

    virtual Table* makeTable() = 0;

    AlgorithmUtilsCommon() : Algorithm( Meta() ), m_domainWorker( NULL ), m_table( NULL ) {}

    ~AlgorithmUtilsCommon() {
        safe_delete( m_table );
    }
};

/// HashSet specialization: HashSet is used as a storage of visited states
template< typename G, typename X = wibble::Unit > struct AlgorithmUtils {};

template< typename G >
struct AlgorithmUtils< G, typename G::Table::IsHashSet >
    : public AlgorithmUtilsCommon< G >
{
    typedef typename G::Table Table;

    Table *makeTable() {
        return new Table( this->hasher, divine::valid< typename G::Node >(),
                          this->equal, this->meta().execution.initialTable );
    }

    AlgorithmUtils() : Algorithm( Meta() ), AlgorithmUtilsCommon< G >() {}
};

#define BITSET BitSet< G, divine::valid< typename G::Node >, algorithm::Equal >

/**
 * BitSet specialization: BitSet with combination of generator::Compact
 * is used as a storage of visited states.
 */
template< typename G >
struct AlgorithmUtils< G, typename G::Table::IsBitSet >
    : AlgorithmUtilsCommon< G >
{
    G* m_g;

    typedef typename G::Table Table;

    Table *makeTable() {
        int id = this->m_domainWorker ? this->m_domainWorker->globalId() : 0;
        return new Table( &this->graph().base(), id,
                          divine::valid< typename G::Node >(), this->equal );
    }

    AlgorithmUtils() : Algorithm( Meta() ), AlgorithmUtilsCommon< G >() {}
};

}
}

#endif
