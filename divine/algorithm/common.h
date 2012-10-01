// -*- C++ -*- (c) 2009 Petr Rockai <me@mornfall.net>

#include <divine/utility/meta.h>
#include <divine/toolkit/blob.h>
#include <divine/toolkit/hashset.h>
#include <divine/toolkit/bitset.h>
#include <divine/toolkit/parallel.h>
#include <divine/toolkit/rpc.h>
#include <divine/graph/visitor.h>
#include <wibble/sfinae.h>

#include <memory>

#ifndef DIVINE_ALGORITHM_H
#define DIVINE_ALGORITHM_H

namespace divine {
namespace algorithm {

struct Hasher {
    int slack;
    uint32_t seed;
    bool allEqual;

    Hasher( int s = 0 ) : slack( s ), seed( 0 ), allEqual( false ) {}
    void setSlack( int s ) { slack = s; }
    void setSeed( uint32_t s ) { seed = s; }

    inline hash_t hash( Blob b ) const {
        assert( b.valid() );
        return b.hash( slack, b.size(), seed );
    }

    inline bool equal( Blob a, Blob b ) const {
        assert( a.valid() );
        assert( b.valid() );
        return allEqual || ( a.compare( b, slack, std::max( a.size(), b.size() ) ) == 0 );
    }

    bool valid( Blob a ) const { return a.valid(); }
};

template< typename _Listener, typename AlgorithmSetup >
struct Visit : AlgorithmSetup, visitor::SetupBase {
    typedef _Listener Listener;
    typedef typename AlgorithmSetup::Graph::Node Node;

    template< typename A, typename V >
    void queueInitials( A &a, V &v ) {
        a.graph().queueInitials( v );
    }
};

struct Algorithm
{
    typedef divine::Meta Meta;

    Meta m_meta;
    int m_slack;

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

    /// Initializes the graph generator by reading a file
    template< typename Self >
    typename Self::Graph *initGraph( Self &self ) {
        typename Self::Graph *g = new typename Self::Graph;
        g->read( meta().input.model );
        g->useProperty( meta().input );
        g->setDomainSize( meta().execution.thisNode,
                          meta().execution.nodes,
                          meta().execution.nodes * meta().execution.threads );
        return g;
    }

    template< typename Self >
    typename Self::Store *initStore( Self &self ) {
        typename Self::Store *s = new typename Self::Store( self.graph() );
        s->hasher().setSeed( meta().algorithm.hashSeed );
        s->hasher().setSlack( self.graph().setSlack( m_slack ) );
        s->id = &self;
        return s;
    }

    template< typename T >
    void ring( typename T::Shared (T::*fun)( typename T::Shared ) ) {
        T *self = static_cast< T * >( this );
        self->shared = self->topology().ring( self->shared, fun );
    }

    template< typename T >
    void parallel( void (T::*fun)() ) {
        T *self = static_cast< T * >( this );

        self->topology().distribute( self->shared, &T::setShared );
        self->topology().parallel( fun );
        self->shareds.clear();
        self->topology().template collect( self->shareds, &T::getShared );
    }

    template< typename Self, typename Setup >
    void visit( Self *self, Setup setup ) {
        visitor::Partitioned< Setup, Self >
            visitor( *self, *self, self->graph(), self->store() );
        setup.queueInitials( *self, visitor );
        visitor.processQueue();
    }

    Algorithm( Meta m, int slack = 0 )
        : m_meta( m ), m_slack( slack )
    {
        want_ce = meta().output.wantCe;
    }

    virtual ~Algorithm() {}
};

template< typename _Setup >
struct AlgorithmUtils
{
    typedef _Setup Setup;
    typedef typename Setup::Store Store;
    typedef typename Setup::Graph Graph;

    std::shared_ptr< Store > m_store;
    std::shared_ptr< Graph > m_graph;

    template< typename Self >
    void init( Self *self ) {
        m_graph = std::shared_ptr< Graph >( self->initGraph( *self ) );
        m_store = std::shared_ptr< Store >( self->initStore( *self ) );
    }

    Graph &graph() {
        assert( m_graph );
        return *m_graph;
    }

    Store &store() {
        assert( m_store );
        return *m_store;
    }
};

}
}

#define ALGORITHM_CLASS(_setup, _shared)                        \
    RPC_CLASS;                                                  \
    typedef _shared Shared;                                     \
    Shared shared;                                              \
    std::vector< Shared > shareds;                              \
    void setShared( Shared sh ) { shared = sh; }                \
    Shared getShared() { return shared; }                       \
    typedef typename _setup::Graph Graph;                       \
    typedef typename _setup::Statistics Statistics;             \
    typedef typename Graph::Node Node;                          \
    typedef typename _setup::Store Store;

#define ALGORITHM_RPC_ID(_type, _id, _fun) \
    template< typename Setup > RPC_ID( _type< Setup >, _fun, 2 + _id )
#define ALGORITHM_RPC(alg) \
    ALGORITHM_RPC_ID(alg, -1, getShared) \
    ALGORITHM_RPC_ID(alg, 0, setShared)

#endif
