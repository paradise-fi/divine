// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <divine/meta.h>
#include <divine/blob.h>
#include <divine/hashset.h>
#include <divine/bitset.h>
#include <divine/visitor.h>
#include <divine/parallel.h>
#include <divine/rpc.h>
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

    template< typename T >
    void ring( typename T::Shared (T::*fun)( typename T::Shared ) ) {
        T *self = static_cast< T * >( this );
        self->shared = self->topology().ring( self->shared, fun );
    }

    template< typename T >
    void parallel( void (T::*fun)() ) {
        T *self = static_cast< T * >( this );

        self->topology().template distribute< decltype( self->shared ) >(
            self->shared, &T::setShared );
        self->topology().parallel( fun );
        self->shareds.clear();
        self->topology().template collect< decltype( self->shareds ), decltype( self->shared ) >(
            self->shareds, &T::getShared );
    }

    Algorithm( Meta m, int slack = 0 )
        : m_meta( m ), m_slack( slack )
    {
        want_ce = meta().output.wantCe;
    }

    virtual ~Algorithm() {}
};

template< typename G >
struct AlgorithmUtilsCommon
{
    typedef typename G::Table Table;

    Table *m_table;
    int *m_tableRefs;
    G *m_graph;

    template< typename Self >
    void init( Self *self ) {
        m_graph = &self->m_graph;
        self->initGraph( m_graph );
        m_table = self->makeTable( self );
        m_tableRefs = new int( 1 );
    }

    G &graph() {
        return *(this->m_graph);
    }

    Table &table() {
        assert( m_table );
        return *m_table;
    }

    AlgorithmUtilsCommon() : m_table( NULL ), m_graph( NULL ) {}
    AlgorithmUtilsCommon( const AlgorithmUtilsCommon &o ) {
        m_table = o.m_table;
        m_tableRefs = o.m_tableRefs;
        m_graph = o.m_graph;
        (*m_tableRefs) ++;
    }
    AlgorithmUtilsCommon &operator=( const AlgorithmUtilsCommon &o ) {
        if (m_tableRefs)
            (*m_tableRefs) --;

        m_table = o.m_table;
        m_tableRefs = o.m_tableRefs;
        m_graph = o.m_graph;
        (*m_tableRefs) ++;
        return *this;
    }

    ~AlgorithmUtilsCommon() {
        (*m_tableRefs)--;
        if (!*m_tableRefs) {
            safe_delete( m_table );
            safe_delete( m_tableRefs );
        }
    }

};

/// HashSet specialization: HashSet is used as a storage of visited states
template< typename G, typename X = wibble::Unit > struct AlgorithmUtils {};

template< typename G >
struct AlgorithmUtils< G, typename G::Table::IsHashSet >
    : public AlgorithmUtilsCommon< G >
{
    typedef typename G::Table Table;

    template< typename Self >
    Table *makeTable( Self *self ) {
        return new Table( self->hasher, divine::valid< typename G::Node >(),
                          self->equal, self->meta().execution.initialTable );
    }
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
    typedef typename G::Table Table;

    template< typename Self >
    Table *makeTable( Self *self ) {
        return new Table( &this->graph().base(), self->id(),
                          divine::valid< typename G::Node >(), self->equal );
    }
};

}
}

#define ALGORITHM_CLASS(_graph, _shared)                        \
    RPC_CLASS;                                                  \
    typedef _shared Shared;                                     \
    Shared shared;                                              \
    std::vector< Shared > shareds;                              \
    void setShared( Shared sh ) { shared = sh; }                \
    Shared getShared() { return shared; }                       \
    _graph m_graph;                                             \
    typedef typename _graph::Node Node;                         \
    typedef typename AlgorithmUtils< _graph >::Table Table;

/* This is ugly, mostly because CPP interprets commas in template parameter
 * lists as argument separators, which means we can't pass multi-parameter
 * templates to RPC_ID. Sigh. */
#define ALGORITHM_RPC_ID(_type, _id, _fun...)                           \
    template< typename G, template < typename > class T, typename S >   \
    template< bool xoxo > struct _type< G, T, S >::RpcId< 2 + _id, xoxo > { \
        typedef decltype( &_type< G, T, S >::_fun ) Fun;                \
        static Fun fun() { return &_type< G, T, S >::_fun; }            \
    };

#define ALGORITHM_RPC(alg) \
    ALGORITHM_RPC_ID(alg, -1, getShared) \
    ALGORITHM_RPC_ID(alg, 0, setShared)

#endif
