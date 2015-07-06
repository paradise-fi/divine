// -*- C++ -*- (c) 2009-2014 Petr Rockai <me@mornfall.net>

#include <memory>
#include <brick-rpc.h>
#include <brick-hlist.h>
#include <brick-types.h>
#include <brick-bitlevel.h>

#include <divine/toolkit/pool.h>
#include <divine/toolkit/parallel.h>
#include <divine/utility/output.h>
#include <divine/utility/meta.h>
#include <divine/graph/visitor.h>

#ifndef DIVINE_ALGORITHM_H
#define DIVINE_ALGORITHM_H

namespace divine {
namespace algorithm {

using brick::bitlevel::BitTuple;
using brick::bitlevel::BitField;
using brick::bitlevel::BitLock;
using std::get;

struct Hasher {
    const int slack;
    uint32_t seed;
    bool allEqual;
    Pool& _pool;

    Hasher( Pool& pool, int s = 0 ) : slack( s ), seed( 0 ), allEqual( false ),
                                      _pool( pool ) {}
    Hasher( Hasher& other, int slack ) : slack( slack ), seed( other.seed ),
                                         allEqual( other.allEqual ), _pool( other._pool )
    { }
    void setSeed( uint32_t s ) { seed = s; }

    Pool &pool() { return _pool; }
    inline hash128_t hash( Blob b ) const {
        ASSERT( _pool.valid( b ) );
        return _pool.hash( b, slack, _pool.size( b ), seed );
    }

    inline bool equal( Blob a, Blob b ) const {
        ASSERT( _pool.valid( a ) );
        ASSERT( _pool.valid( b ) );
        return allEqual || _pool.equal( a, b, slack );
    }

    bool valid( Blob a ) const { return _pool.valid( a ); }
    bool alias( Blob a, Blob b ) { return _pool.alias( a, b ); }
};

template< typename _Listener, typename _AlgorithmSetup >
struct Visit : _AlgorithmSetup, visitor::SetupBase {
    typedef _Listener Listener;
    typedef _AlgorithmSetup AlgorithmSetup;
    typedef typename AlgorithmSetup::Graph::Node Node;
    typedef typename AlgorithmSetup::Graph::Label Label;
    using Vertex = typename AlgorithmSetup::Store::Vertex;

    template< typename A, typename V >
    void queueInitials( A &a, V &v ) {
        a.graph().initials( a.store().alloc, [ &a, &v ] ( Node, Node n, Label l ) {
                v.queue( Vertex(), n, l );
            } );
    }

    template< typename A, typename V >
    void queuePOR( A &a, V &v ) {
        a.graph().porExpand( a.store().alloc, a.store(), [ &a, &v ] ( Vertex f, Node n, Label l ) {
                v.queueAny( f, n, l );
            } );
    }
};

template< typename T >
struct Locally {
    T *t;
    Locally( T &t ) : t( &t ) {}
    Locally() : t( nullptr ) {}
};

template< typename BS, typename T >
typename BS::bitstream &operator>>( BS &bs, Locally< T > & )
{
    return bs;
}

template< typename BS, typename T >
typename BS::bitstream &operator<<( BS &bs, const Locally< T > & )
{
    return bs;
}

struct Algorithm
{
    typedef divine::Meta Meta;

    Meta m_meta;
    const int m_slack;
    int m_slack_adjusted;

    meta::Result &result() { return meta().result; }

    virtual void run() = 0;

    bool want_ce;

    Meta &meta() {
        return m_meta;
    }

    std::ostream &progress() {
        return Output::output().progress();
    }

    void resultBanner( bool valid ) {
        progress() << " ============================================= " << std::endl
                   << ( valid ?
                        ( result().fullyExplored == meta::Result::R::Yes ?
                          "               The property HOLDS          " :
                          "     The property HOLDS (approximation)    " ) :
                          "           The property DOES NOT hold      " )
                   << std::endl
                   << " ============================================= " << std::endl;
    }

    /// Initializes the graph generator by reading a file
    template< typename Self >
    typename Self::Graph *initGraph( Self &self, typename Self::Setup::Generator *master_g = nullptr, bool full = true ) {
        typename Self::Graph *g = new typename Self::Graph;
        g->setPool( self.masterPool() );
        m_slack_adjusted = g->setSlack( m_slack );
        g->read( meta().input.model, meta().input.definitions, master_g );
        if ( full ) {
            g->useProperties( meta().input.properties );
            meta().algorithm.reduce =
                g->useReductions( meta().algorithm.reduce );
            g->fairnessEnabled( meta().algorithm.fairness );
            g->demangleStyle( meta().algorithm.demangle );
        }
        return g;
    }

    template< typename Self >
    typename Self::Store *initStore( Self &self, Self* master ) {
        typename Self::Store *s =
            new typename Self::Store( self.graph(), m_slack_adjusted,
                    master ? &master->store() : nullptr );
        s->hasher().setSeed( meta().algorithm.hashSeed );
        s->setSize( meta().execution.initialTable );
        s->setId( self );
        return s;
    }

    template< typename Val, typename T >
    Val ring( Val val, Val (T::*fun)( Val ) ) {
        T *self = static_cast< T * >( this );
        return self->topology().ring( std::move( val ), fun );
    }

    template< typename T >
    void ring( typename T::Shared (T::*fun)( typename T::Shared ) ) {
        T *self = static_cast< T * >( this );
        self->shared = self->topology().ring( self->shared, fun );
    }

    template< typename T >
    void parallel( void (T::*fun)() )
    {
        T *self = static_cast< T * >( this );

        self->_visitorData.create();
        self->topology().distribute( Locally< typename T::VisitorData >( self->_visitorData ), &T::setVisitorData );
        self->topology().distribute( self->shared, &T::setShared );
        self->topology().parallel( fun );
        self->shareds.clear();
        self->topology().template collect( self->shareds, &T::getShared );
        self->_visitorData = typename T::VisitorData();
    }

    template< typename Self, typename Setup >
    auto makeVisitor( typename Setup::Listener &l, Self &self, Setup )
        -> typename Setup::Visitor::template Implementation< Setup, Self >
    {
        return typename Setup::Visitor::template Implementation< Setup, Self >(
            l, self, self.graph(), self.store(), self._visitorData );
    }

    template< typename Self, typename Setup >
    void visit( Self *self, Setup setup, bool por = false ) {
        auto visitor = self->makeVisitor( *self, *self, setup );
        if ( por )
            setup.queuePOR( *self, visitor );
        else
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

using brick::types::Unit;

template< typename _Setup, typename _Shared = Unit >
struct AlgorithmUtils {
    typedef _Setup Setup;
    typedef typename Setup::Store Store;
    typedef typename Setup::Graph Graph;


    typedef _Shared Shared;
    Shared shared;
    std::vector< Shared > shareds;
    void setShared( Shared sh ) { shared = sh; }
    Shared getShared() { return shared; }

    std::shared_ptr< Store > m_store;
    std::shared_ptr< Graph > m_graph;

    using This = AlgorithmUtils< Setup, Shared >;
    using VisitorData = typename Setup::Visitor::template Data< Setup >;

    VisitorData _visitorData;
    void setVisitorData( Locally< VisitorData > d ) { _visitorData = *d.t; }

    BRICK_RPC( rpc::Root, &This::getShared, &This::setShared, &This::setVisitorData );

    template< typename Self >
    void init( Self &self, Self &master, std::pair< int, int > id ) {
        self.becomeSlave( master.topology(), id );
        _init( self, &master, nullptr );
    }

    template< typename Self >
    void init( Self &self, typename Setup::Generator *g ) {
        self.becomeMaster( self.meta().execution.threads );
        _init( self, static_cast< Self * >( nullptr ), g );
        self.initSlaves( self );
    }

    template< typename Self >
    void _init( Self &self, Self* master, typename Setup::Generator *g ) {
        static_assert( std::is_base_of< AlgorithmUtils< Setup, Shared >, Self >::value,
               "Algorithm must be descendant of AlgorithmUtils" );
        ASSERT_EQ( static_cast< Self* >( this ), &self );

        m_graph = std::shared_ptr< Graph >( self.initGraph( self, master ? &master->graph().base() : g ) );
        m_store = std::shared_ptr< Store >( self.initStore( self, master ) );
    }

    Graph &graph() {
        ASSERT( m_graph );
        return *m_graph;
    }

    Pool& pool() {
        return graph().pool();
    }

    Store &store() {
        ASSERT( m_store );
        return *m_store;
    }
};

}
}

#define ALGORITHM_CLASS(_setup)                                 \
    typedef typename _setup::Graph Graph;                       \
    typedef typename _setup::Statistics Statistics;             \
    typedef typename Graph::Node Node;                          \
    typedef typename Graph::Label Label;                        \
    typedef typename _setup::Store Store;                       \
    typedef typename Store::Hasher Hasher;                      \
    typedef typename Setup::Store::Handle Handle;               \
    typedef typename Setup::Store::Vertex Vertex

#endif
