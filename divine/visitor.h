// -*- C++ -*-

#include <fstream>
#include <vector>

#include <divine/hashmap.h>
#include <divine/pool.h>
#include <divine/blob.h>
#include <divine/fifo.h>
#include <divine/generator.h>
#include <divine/legacy/por/por.hh>
#include <divine/threading.h>
#include <divine/bundle.h>

#ifndef DIVINE_VISITOR_H
#define DIVINE_VISITOR_H

namespace divine {
namespace visitor {

enum TransitionAction { ExpandTransition, // force expansion on the target state
                        FollowTransition, // expand the target state if it's
                                          // not been expanded yet
                        IgnoreTransition, // pretend the transition didn't exist
                        TerminateOnTransition
};
enum ExpansionAction { ExpandState };

template<
    typename G, // graph
    typename N, // notify
    TransitionAction (N::*tr)(typename G::Node, typename G::Node) = &N::transition,
    ExpansionAction (N::*exp)(typename G::Node) = &N::expansion >
struct Setup {
    typedef G Graph;
    typedef N Notify;
    typedef typename Graph::Node Node;
    static TransitionAction transition( Notify &n, Node a, Node b ) {
        return (n.*tr)( a, b );
    }

    static ExpansionAction expansion( Notify &n, Node a ) {
        return (n.*exp)( a );
    }
};

template< typename T >
struct Queue {
    std::deque< T > m_queue;
    void push( const T &t ) { m_queue.push_back( t ); }
    void pop() { m_queue.pop_front(); }
    T &next() { return m_queue.front(); }
    bool empty() { return m_queue.empty(); }
};

template< typename T >
struct Stack {
    std::deque< T > m_stack;
    void push( const T &t ) { m_stack.push_back( t ); }
    void pop() { m_stack.pop_back(); }
    T &next() { return m_stack.back(); }
    bool empty() { return m_stack.empty(); }
};

// the _ suffix is to avoid clashes with the old-style visitors
template<
    template< typename > class Queue, typename S >
struct Common_ {
    typedef typename S::Graph Graph;
    typedef typename S::Node Node;
    typedef typename S::Notify Notify;
    typedef typename Graph::Successors Successors;
    Queue< Successors > m_queue;
    typedef HashMap< Node, Unit > Seen;
    Graph &m_graph;
    Notify &m_notify;
    Seen *m_seen;

    Seen &seen() {
        return *m_seen;
    }

    void visit() {
        assert( 0 );
    }

    void visit( Node initial ) {
        TransitionAction tact;
        ExpansionAction eact;
        if ( seen().has( initial ) )
            return;
        seen().insert( initial );
        S::expansion( m_notify, initial );
        m_queue.push( m_graph.successors( initial ) );
        while ( !m_queue.empty() ) {
            Successors &s = m_queue.next();
            if ( s.empty() ) {
                // finished with s
                m_queue.pop();
                continue;
            } else {
                Node current = s.head();
                s = s.tail();

                int usage = seen().usage();
                current = seen().insert( current ).key;
                bool had = usage == seen().usage(); // not a new state...

                tact = S::transition( m_notify, s.from(), current );
                if ( tact == ExpandTransition ||
                     (tact == FollowTransition && !had) ) {
                    eact = S::expansion( m_notify, current );
                    if ( eact == ExpandState )
                        m_queue.push( m_graph.successors( current ) );
                }
            }
        }
    }

    Common_( Graph &g, Notify &n, Seen *s ) :
        m_graph( g ), m_notify( n ), m_seen( s )
    {
        if ( !m_seen )
            m_seen = new Seen();
    }
};

template< typename S >
struct BFV : Common_< Queue, S > {
    typedef HashMap< typename S::Node, Unit > Seen;
    BFV( typename S::Graph &g, typename S::Notify &n, Seen *s = 0 )
        : Common_< Queue, S >( g, n, s ) {}
};

template< typename S >
struct DFV : Common_< Stack, S > {
    typedef HashMap< typename S::Node, Unit > Seen;
    DFV( typename S::Graph &g, typename S::Notify &n, Seen *s = 0 )
        : Common_< Stack, S >( g, n, s ) {}
};

template< typename S, typename Domain >
struct Parallel {
    typedef typename S::Node Node;

    Domain &dom;
    typename S::Notify &notify;
    typename S::Graph &graph;
    typedef HashMap< Node, Unit > Seen;

    Seen *m_seen;

    int owner( Node n ) const {
        return unblob< int >( n ) % dom.peers();
    }

    visitor::TransitionAction transition( Node f, Node t ) {
        if ( owner( t ) != dom.id() ) {
            Fifo< Blob > &fifo = dom.queue( owner( t ) );
            MutexLock __l( fifo.writeMutex );
            fifo.push( __l, unblob< Node >( f ) );
            fifo.push( __l, unblob< Node >( t ) );
            return visitor::IgnoreTransition;
        }
        return S::transition( notify, f, t );
    }
    
    visitor::ExpansionAction expansion( Node n ) {
        assert_eq( owner( n ), dom.id() );
        return S::expansion( notify, n );
    }

    template< typename BFV >
    void run( BFV &bfv ) {
        while ( true ) {
            if ( dom.fifo.empty() ) {
                if ( dom.master().m_barrier.idle( &dom ) )
                    return;
            } else {
                Node f, t;
                f = dom.fifo.front();
                dom.fifo.pop();
                t = dom.fifo.front( true );
                dom.fifo.pop();

                S::transition( notify, f, t );
                bfv.visit( unblob< Node >( t ) );
            }
        }
    }

    void visit( Node initial ) {
        typedef Setup< typename S::Graph, Parallel< S, Domain > > Ours;
        BFV< Ours > bfv( graph, *this, m_seen );
        if ( initial.valid() && owner( initial ) == dom.id() ) {
            bfv.visit( unblob< Node >( initial ) );
        }
        run( bfv );
    }

    void visit() {
        return visit( Node() ); // assuming Node().valid() == false
    }

    Parallel( typename S::Graph &g, Domain &d, typename S::Notify &n, Seen *s = 0 )
        : dom( d ), notify( n ), graph( g ), m_seen( s )
    {}
};

namespace impl {

template< typename Bundle, typename Self >
struct Common
{
    typedef typename Bundle::State State;
    typedef typename Bundle::Storage Storage;
    typedef typename Bundle::Controller Controller;
    typedef typename Bundle::Generator Generator;

    Controller &m_controller;

    succ_container_t m_succs;
    Generator sys;
    // dve_explicit_system_t sys;
    int m_seenCount, m_genCount;
    std::string file;
    uint16_t m_seenFlag, m_pushedFlag;

    Controller &controller() { return m_controller; }
    Storage &storage() { return controller().storage(); }
    typename Bundle::Observer &observer() { return controller().observer(); }

    Self &self() { return *static_cast< Self* >( this ); }

    // call self().transition( from, to ) for each successor
    void processSuccessors( State from )
    {
        typename Generator::Successors s = sys.successors( from );
        while ( !s.empty() ) {
            ++ m_genCount;
            self().transition( from, s.head() );
            s = s.tail();
        }
        if ( s.result() != SUCC_NORMAL ) {
            self().errorState( from, s.result() );
        }
    }

    State ignore( State st )
    {
        return State();
    }

    State follow( State st )
    {
        State r = State();
        if ( !self().queued( st ) ) {
            r = self().push( st );
            self().markQueued( r );
        }
        return r;
    }

    typename Storage::Reference merge( State st )
    {
        typename Storage::Reference ret = self().storage().insert( st );
        if ( ret.key.pointer() != st.pointer() )
            st.setDeleteFlag();
        return ret;
    }

    void visitFromInitial()
    {
        self().visit( self().sys.initial() );
    }

    void visit( State initial )
    {
        typename Storage::Reference r = self().merge( initial );
        self().push( r.key );
        self().visit();
    }

    void stateWork( State st )
    {
        if ( !self().observer().stateWork( st ) )
            visit( st );
    }

    void finished( State st ) {
        this->observer().finished( st );
    }

    void expanding( State s ) {
        self().observer().expanding( s );
    }

    void errorState( State s, int err ) {
        self().observer().errorState( s, err );
    }

    State transition( State from, State to ) {
        return self().controller().transition( from, to );
    }

    State localTransition( State from, State to ) {
        return self().observer().transition( from, to );
    }

    bool seen( State st ) {
        return st.flags() & m_seenFlag;
    }

    void markSeen( State st ) {
        st.setFlags( st.flags() | m_seenFlag );
    }

    bool queued( State st ) {
        return st.flags() & m_pushedFlag;
    }

    void markQueued( State st ) {
        st.setFlags( st.flags() | m_pushedFlag );
    }

    void visit()
    // TODO remove storage locking please (we currently need it here to
    // manipulate flags safely, apparently)
    {
        self().observer().started(); // XXX
        if ( self().empty() )
            return;
        while ( !self().empty() ) {
            typename Storage::Reference ref = self().storage().get( self().next() );

            State state = ref.key;
            self().remove();
            assert( state.valid() );

            if ( self().seen( state ) ) {
                continue;
            }

            self().expanding( state );

            self().markSeen( state );
            assert( self().seen( state ) );

            self().processSuccessors( state );
        }
    }

    void setSeenFlag( size_t f ) {
        m_seenFlag = f;
    }

    void setPushedFlag( size_t f ) {
        m_pushedFlag = f;
    }

    void initSystem()
    {
        sys.setAllocator( controller().storage().newAllocator() );
        sys.read( controller().config().input() );
    }

    Common( Controller &c ) :
        m_controller( c ), m_seenCount( 0 )
    {
        m_seenFlag = SEEN;
        m_pushedFlag = PUSHED;
        m_genCount = m_seenCount = 0;
    }
};

template< typename Bundle, typename Self >
struct Por : Common< Bundle, Self >
{
    typedef typename Bundle::State State;

    por_t por;

    void processSuccessors( State from )
    {
        size_t proc_gid; // output parameter... what for?
        por.ample_set_succs( from.state(), this->m_succs, proc_gid );
        for ( size_t i = this->m_succs.size(); i > 0; i-- ) {
            ++ this->m_genCount;
            state_t _st = this->m_succs[ i - 1 ];
            State to = State( _st );
            this->self().transition( from, to );
        }
        this->m_succs.clear();
    }

    void initSystem()
    {
        Common< Bundle, Self >::initSystem();
        por.init( &(this->sys) );
        por.set_choose_type( POR_SMALLEST );
    }

    Por( typename Bundle::Controller &c ) : Common< Bundle, Self >( c )
    {
    }
};

template< template< typename, typename > class Inherit = Common >
struct Order {

    template< typename Bundle, typename Self >
    struct DFS
        : Inherit< Bundle, DFS< Bundle, Self > >
    {
        typedef typename Bundle::State State;
        typedef std::deque< State > Stack;

        int m_height;
        Stack m_stack;
        std::deque< bool > m_postStack;

        int height() { return m_height; }
        bool empty() { return m_stack.empty(); }
        State next() { return m_stack.back(); }
        void remove() {
            if ( m_postStack.back() ) {
                --m_height;
                this->self().finished( m_stack.back() );
            }

            if ( !m_stack.empty() ) { // finished() is allowed to clear stack
                m_stack.pop_back();
                m_postStack.pop_back();
            }
        }

        State push( State st ) {
            assert( st.valid() );
            m_stack.push_back( st );
            if ( !(st.flags() & this->m_pushedFlag ) ) {
                m_postStack.push_back( true );
                st.setFlags( st.flags() | this->m_pushedFlag );
                ++ m_height;
            } else
                m_postStack.push_back( false );
            return st;
        }

        void clear() {
            m_stack.clear();
            m_postStack.clear();
        }

        DFS( typename Bundle::Controller &m )
            : Inherit< Bundle, DFS >( m ), m_height( 0 )
        {
            this->initSystem();
        }
    };

    template< typename Bundle, typename Self >
    struct BFS
        : Inherit< Bundle, BFS< Bundle, Self > >
    {
        typedef typename Bundle::State State;
        typedef std::deque< State > Queue;

        Queue m_queue;

        bool empty() { return m_queue.empty(); }
        State next() { return m_queue.front(); }
        void remove() {
            m_queue.pop_front();
        }

        State push( State state ) {
            m_queue.push_back( state );
            return state;
        }

        void clear() {
            m_queue.clear();
        }

        BFS( typename Bundle::Controller &m )
            : Inherit< Bundle, BFS >( m )
        {
            this->initSystem();
        }
    };

};

}

typedef Finalize< impl::Order<>::DFS > DFS;
typedef Finalize< impl::Order<>::BFS > BFS;

typedef Finalize< impl::Order< impl::Por >::DFS > PorDFS;
typedef Finalize< impl::Order< impl::Por >::BFS > PorBFS;

}
}

#endif
