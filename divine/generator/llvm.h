// -*- C++ -*- (c) 2011-2014 Petr Rockai <me@mornfall.net>

#ifdef O_LLVM
#include <stdint.h>
#include <iostream>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <divine/llvm/wrap/Constants.h>
#include <divine/llvm/wrap/Module.h>

#pragma GCC diagnostic pop

#include <divine/generator/common.h>
#include <divine/llvm/interpreter.h>
#include <divine/ltl2ba/main.h>
#include <divine/toolkit/lens.h>

#include <wibble/exception.h>

#ifndef DIVINE_GENERATOR_LLVM_H
#define DIVINE_GENERATOR_LLVM_H

namespace divine {
namespace generator {

namespace machine = llvm::machine;
using namespace ::llvm;

template< typename _Label, typename HeapMeta >
struct _LLVM : Common< Blob > {
    typedef Blob Node;
    using Interpreter = llvm::Interpreter< HeapMeta >;

    std::shared_ptr< divine::llvm::BitCode > bitcode;
    Interpreter *_interpreter;
    std::shared_ptr< Interpreter > _interpreter_2;
    Node _initial;
    typedef _Label Label;

    typedef std::vector< int > PropGuard;
    typedef std::pair< PropGuard, int > PropTrans;

    std::vector< std::vector< int > > prop_next;
    std::vector< PropTrans > prop_trans;
    std::vector< bool > prop_accept;
    int prop_initial;
    ReductionSet reduce;

    bool use_property;
    std::vector< llvm::Problem::What > goalProblems;

    graph::DemangleStyle demangle;

    template< typename Yield >
    void initials( Yield yield )
    {
        interpreter();
        Function *f = bitcode->module->getFunction( "_divine_start" );
        bool is_start = true;
        if ( !f ) {
            f = bitcode->module->getFunction( "main" );
            is_start = false;
        }
        if ( !f )
            die( "FATAL: Missing both _divine_start and main in verified model." );
        yield( Node(), interpreter().initial( f, is_start ), Label() );
    }

    bool buchi_enabled( PropGuard guard, unsigned ap ) {
        for ( auto g : guard ) {
            if ( g < 0 && (ap & (1 << (-g - 1))) ) return false;
            if ( g > 0 && !(ap & (1 << ( g - 1))) ) return false;
        }
        return true;
    }

    template< typename Yield >
    void successors( Node st, Yield yield ) {
        if ( !use_property )
            return interpreter().run( st, [&]( Node n, llvm::Label p ) { yield( n, Label( p ) ); } );
        interpreter().run( st, [&]( Node n, llvm::Label p ){
                std::vector< int > buchi_succs;
                for ( auto next : this->prop_next[ this->flags( n ).buchi ] ) {
                    auto trans = this->prop_trans[ next ];
                    if ( this->buchi_enabled( trans.first, this->flags( n ).ap ) )
                        buchi_succs.push_back( trans.second );
                }

                if ( buchi_succs.empty() ) {
                    this->release( n );
                    return;
                }

                while ( buchi_succs.size() > 1 ) {
                    Blob b = this->pool().allocate( this->pool().size( n ) );
                    this->pool().copy( n, b );
                    this->flags( b ).buchi = buchi_succs.back();
                    buchi_succs.pop_back();
                    yield( b, Label() );
                }

                this->flags( n ).buchi = buchi_succs.front();
                yield( n, Label( p ) ); /* TODO? */
            } );
    }

    void release( Node s ) {
        pool().free( s );
    }

    using MachineState = llvm::MachineState<>;
    using State = MachineState::State;
    using Flags = llvm::machine::Flags;
    using Heap = llvm::machine::Heap;

    template< typename T >
    T &state( Blob b ) { return interpreter().state.state( b ).get( T() ); }
    Flags &flags( Blob b ) { return state< Flags >( b ); }

    bool isGoal( Node n ) {
        auto &fl = state< Flags >( n );
        for ( int i = 0; i < fl.problemcount; ++i )
            for ( auto p : goalProblems )
                if ( fl.problems( i ).what == p )
                    return true;
        return false;
    }

    bool isAccepting( Node n ) {
        if ( !use_property )
            return false;
        return prop_accept[ flags( n ).buchi ];
    }

    template< typename Coroutine >
    void splitHint( Coroutine &cor, int a, int b, int ch ) {
        Common::splitHint( cor, a, b, ch );
    }

    template< typename Coroutine >
    void splitHint( Coroutine &cor ) {
        using L = lens::Lens< machine::StateAddress, MachineState::State >;
        auto state = L(
            machine::StateAddress( &this->pool(), this->bitcode->info,
                                   cor.item, this->_slack ) );

        enum Ty { NA, Flags, Globals, Heap, HeapM, Threads };
        using Chunk = std::pair< Ty, int >;
        using Chunks = std::deque< Chunk >;

        /* all these include slack in their offsets */
        Chunks starts{
             Chunk( Flags, state.address( machine::Flags() ).offset ),
             Chunk( Globals, state.address( machine::Globals() ).offset ),
             Chunk( Heap, state.address( machine::Heap() ).offset ),
             Chunk( HeapM, state.address( MachineState::HeapMeta() ).offset ),
             Chunk( Threads, state.address( MachineState::Threads() ).offset ) },
            ends = starts;

        ends.pop_front();
        ends.push_back( Chunk( NA, pool().size( cor.item ) ) );
        Chunks chunks;

        auto end = ends.begin();
        for ( auto ch : starts ) {
            if ( end->second - ch.second > 0 )
                chunks.emplace_back( ch.first, end->second - ch.second );
            ++end;
        }

        cor.split( chunks.size() );

        for ( auto ch : chunks )
            Common::splitHint( cor, ch.second, 0 );

        cor.join();
    }

    std::string showConstdata() {
        return interpreter().describeConstdata();
    }

    std::string showNode( Node n ) {
        interpreter(); /* ensure _interpreter_2 is initialised */
        _interpreter_2->rewind( n );
        std::string s = _interpreter_2->describe( demangle == DemangleStyle::Cpp );
        if ( use_property ) {
            int buchi = _interpreter_2->state.flags().buchi;
            s += "LTL: " + wibble::str::fmt( buchi ) + " (";
            for ( int i = 0; i < int( prop_next[ buchi ].size() ); ++i ) {
                int next = prop_next[buchi][i];
                s += wibble::str::fmt( prop_trans[next].first ) + " -> " +
                     wibble::str::fmt( prop_trans[next].second ) + "; ";
            }
            s += ")\n";
        }
        return s;
    }

    std::string showTransition( Node, Node, Label l ) {
        return graph::showLabel( l );
    }

    void die( std::string err ) __attribute__((noreturn)) {
        std::cerr << err << std::endl;
        exit( 1 );
    }

    void read( std::string file, std::vector< std::string > /* definitions */,
               _LLVM *blueprint = nullptr )
    {
        if ( blueprint )
            bitcode = blueprint->bitcode;
        else
            bitcode = std::make_shared< divine::llvm::BitCode >( file );
    }

    template< typename Y >
    void properties( Y yield ) {
        yield( "deadlock", "deadlock freedom", PT_Deadlock );
        yield( "pointsto", "verify points-to info stored in LLVM metadata", PT_Goal );
        yield( "assert", "assertion safety", PT_Goal );
        yield( "memory", "memory safety (invalid dereferences + bound checks)", PT_Goal );
        yield( "arithmetic", "arithmetic safety (division by zero)", PT_Goal );
        yield( "leak", "memory leak freedom", PT_Goal );
        yield( "user", "user or library defined safety problems", PT_Goal );
        yield( "guard", "safety of compiler-defined guards (unreachable)", PT_Goal );
        yield( "mutex", "mutex deadlock safety", PT_Goal );
        yield( "safety", "[assert + memory + arithmetic + leak + user + guard + mutex]", PT_Goal );
        for ( auto p : interpreter().properties )
            yield( p.first, p.second, PT_Buchi );
    }

    int literal_id( std::string lit ) {
        MDNode *ap = interpreter().findEnum( "APs" );
        if ( !ap )
            die( "FATAL: atomic proposition names could not be detected.\n"
                    "Maybe you are missing enum APs." );
        for ( int i = 0; i < int( ap->getNumOperands() ); ++i ) {
            MDNode *it = cast< MDNode >( ap->getOperand(i) );
            MDString *name = cast< MDString >( it->getOperand(1) );
            if ( name->getString() == lit )
                return 1 + cast< ConstantInt >( it->getOperand(2) )->getValue().getZExtValue();
        }
        assert_die();
    }

    void useProperty( llvm::Problem::What w ) {
        if ( std::find( goalProblems.begin(), goalProblems.end(), w ) != goalProblems.end() )
            return;
        goalProblems.push_back( w );
    }

    bool useProperty( std::string name )
    {
        size_t sz = goalProblems.size();

        if ( name == "pointsto" )
            useProperty( llvm::Problem::PointsToViolated );

        if ( name == "assert" || name == "safety" )
            useProperty( llvm::Problem::Assert );

        if ( name == "memory" || name == "safety" ) {
            useProperty( llvm::Problem::InvalidDereference );
            useProperty( llvm::Problem::OutOfBounds );
        }

        if ( name == "arithmetic" || name == "safety" )
            useProperty( llvm::Problem::DivisionByZero );

        if ( name == "leak" || name == "safety" )
            useProperty( llvm::Problem::MemoryLeak );

        if ( name == "user" || name == "safety" ) {
            useProperty( llvm::Problem::Other );
            useProperty( llvm::Problem::NotImplemented );
        }

        if ( name == "guard" || name == "safety" ) {
            useProperty( llvm::Problem::InvalidArgument );
            useProperty( llvm::Problem::NotImplemented );
        }

        if ( name == "mutex" || name == "safety" )
            useProperty( llvm::Problem::Deadlock );

        return goalProblems.size() > sz;
    }

    void useProperties( PropertySet s ) {
        std::string ltl;
        auto &props = interpreter().properties;

        for ( std::string name : s ) {
            if ( name == "deadlock" ) {
                assert_eq( s.size(), 1u );
                return;
            }

            if ( useProperty( name ) )
                continue;

            if ( props.count( name ) )
                ltl += (ltl.empty() ? "" : " && ") + props[ name ];
            else
                throw wibble::exception::Consistency(
                    "Unknown property " + name + ". Please consult divine info." );
        }

        if ( ltl.empty() )
            return;

        use_property = true;
        BA_opt_graph_t b = buchi( ltl );

        typedef std::list< KS_BA_node_t * > NodeList;
        typedef std::list< BA_trans_t > TransList;
        std::set< std::string > lits;

        NodeList nodes, initial, accept;
        nodes = b.get_all_nodes();
        initial = b.get_init_nodes();
        accept = b.get_accept_nodes();

        assert_eq( initial.size(), 1U );
        prop_initial = initial.front()->name;
        prop_next.resize( nodes.size() );
        prop_accept.resize( nodes.size(), false );

        for ( NodeList::iterator n = nodes.begin(); n != nodes.end(); ++n ) {
            assert_leq( (*n)->name, int( nodes.size() ) );
            int nid = (*n)->name - 1;

            for ( TransList::const_iterator t = b.get_node_adj(*n).begin();
                  t != b.get_node_adj(*n).end(); ++t ) {
                std::vector< int > guard;

                for ( LTL_label_t::const_iterator l = t->t_label.begin();
                      l != t->t_label.end(); ++l ) {
                    assert_leq( 1, literal_id( l->predikat ) );
                    /* NB. Negation implies a *positive* query on an AP. */
                    guard.push_back( (l->negace ? 1 : -1) * literal_id( l->predikat ) );
                }
                prop_next[nid].push_back( prop_trans.size() );
                prop_trans.push_back( std::make_pair( guard, t->target->name - 1 ) );
            }
        }
        for ( NodeList::iterator n = accept.begin(); n != accept.end(); ++n )
            prop_accept[(*n)->name - 1] = true;
    }

    void applyReductions() {
        if ( reduce.count( R_Tau ) )
            interpreter().tauminus = true;
        if ( reduce.count( R_TauPlus ) ) {
            interpreter().tauminus = true;
            interpreter().tauplus = true;
        }
        if ( reduce.count( R_TauStores ) ) {
            interpreter().tauminus = true;
            interpreter().taustores = true;
        }
    }

    ReductionSet useReductions( ReductionSet r ) {
        if ( r.count( R_Tau ) )
            reduce.insert( R_Tau );
        if ( r.count( R_TauPlus ) )
            reduce.insert( R_TauPlus );
        if ( r.count( R_TauStores ) )
            reduce.insert( R_TauStores );

        reduce.insert( R_Heap );

        if ( _interpreter )
            applyReductions();

        return reduce;
    }

    Interpreter &interpreter() {
        if (_interpreter)
            return *_interpreter;

        _interpreter = new Interpreter( this->pool(), this->_slack, bitcode );
        if ( !_interpreter_2 )
            _interpreter_2 = std::make_shared< Interpreter >( this->pool(), this->_slack, bitcode );
        applyReductions();

        return *_interpreter;
    }

    void demangleStyle( graph::DemangleStyle st ) {
        demangle = st;
    }

    _LLVM() : _interpreter( 0 ), use_property( false ) {}
    _LLVM( const _LLVM &other ) {
        *this = other;
        _interpreter = 0;
        _initial = Node();
    }

    ~_LLVM() {
        delete _interpreter;
    }

};

typedef _LLVM< graph::NoLabel, llvm::machine::NoHeapMeta > LLVM;
typedef _LLVM< graph::NoLabel, llvm::machine::HeapIDs > PointsToLLVM;
typedef _LLVM< graph::Probability, llvm::machine::NoHeapMeta > ProbabilisticLLVM;
typedef _LLVM< graph::ControlLabel, llvm::machine::NoHeapMeta > ControlLLVM;

}
}

#endif
#endif
