// -*- C++ -*- (c) 2011-2014 Petr Rockai <me@mornfall.net>

#if GEN_LLVM
#include <stdint.h>
#include <iostream>
#include <stdexcept>

#include <brick-string.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Module.h>

#pragma GCC diagnostic pop

#include <divine/generator/common.h>
#include <divine/llvm/interpreter.h>
#include <divine/ltl2ba/main.h>
#include <divine/toolkit/lens.h>


#ifndef DIVINE_GENERATOR_LLVM_H
#define DIVINE_GENERATOR_LLVM_H

namespace divine {
namespace generator {

namespace machine = llvm::machine;
using namespace ::llvm;

enum class LLVMSplitter { Generic, Hybrid, PerObject };

template< typename _Label, typename HeapMeta, LLVMSplitter LS = LLVMSplitter::PerObject >
struct _LLVM : Common< Blob > {
    typedef Blob Node;
    using Interpreter = llvm::Interpreter< HeapMeta, _Label >;

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
    brick::data::Bimap< int, std::string > apNames;
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
            return interpreter().run( st, yield );
        interpreter().run( st, [&]( Node n, Label p ){
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
                yield( n, p ); /* TODO? */
            } );
    }

    void release( Node s ) {
        pool().free( s );
    }

    using MachineState = llvm::MachineState< HeapMeta >;
    using State = typename MachineState::State;
    using StateLens = typename MachineState::StateLens;
    using Threads = typename MachineState::Threads;
    using Flags = llvm::machine::Flags;
    using Heap = llvm::machine::Heap;

    StateLens state( Blob b ) { return interpreter().state.state( b ); }
    Flags &flags( Blob b ) { return state( b ).get( Flags() ); }

    template< typename Yield >
    void enumerateFlags( Yield yield ) {
        int i = graph::flags::firstAvailable;
        for ( int p = 0; p < llvm::Problem::End; ++p, ++i )
            yield( std::to_string( llvm::Problem::What( p ) ), i,
                    p == llvm::Problem::PointsToViolated
                    ? graph::flags::Type::Goal : graph::flags::Type::DefaultGoal );
        for ( auto ap : apNames.left() )
            yield( ap.second, i + ap.first, graph::flags::Type::Proposition );
    }

    template< typename QueryFlags >
    graph::FlagVector stateFlags( Node n, QueryFlags qf ) {
        auto &fl = flags( n );
        graph::FlagVector out;
        const int apOffset = graph::flags::firstAvailable + llvm::Problem::End;

        for ( auto f : qf ) {
            if ( f == graph::flags::accepting ) {
                if ( use_property && prop_accept[ fl.buchi ] )
                    out.emplace_back( f );
            } else if ( f == graph::flags::goal ) {
                for ( int i = 0; i < fl.problemcount; ++i )
                    for ( auto p : goalProblems )
                        if ( fl.problems( i ).what == p )
                            out.emplace_back( f );
            } else if ( f < apOffset ) {
                for ( int i = 0; i < fl.problemcount; ++i )
                    if ( fl.problems( i ).what == f - graph::flags::firstAvailable ) {
                        out.emplace_back( f );
                        break;
                    }
            } else if ( fl.ap & (1 << (f - apOffset)) )
                 out.emplace_back( f );
        }
        return std::move( out );
    }

    template< typename Coroutine >
    void splitHint( Coroutine &cor, int a, int b, int ch ) {
        Common::splitHint( cor, a, b, ch );
    }

    enum class ChunkT { NA, Flags, Globals, Heap, HeapM, Threads };
    using Chunk = std::pair< ChunkT, int >;
    using Chunks = std::deque< Chunk >;
    using Levels = std::deque< int >;

    template< typename Coroutine >
    void splitAccumulate( Coroutine &cor, const Chunks &ch, const Levels &ls,
                          int from, int to, int l )
    {
        ASSERT_LEQ( 1, to - from );
        if ( to - from == 1 ) {
            cor.consume( ch[ from ].second );
            return;
        }
        int deg = l < ls.size() ? ls[ l ] : to - from, w = (to - from) / deg;
        w += (to - from) % deg > 0 ? 1 : 0;
        cor.split( (to - from) / w + ( (to - from) % w ? 1 : 0 ) );
        for ( int i = from; i < to; i += w )
            splitAccumulate( cor, ch, ls, i, std::min( i + w, to ), l + 1 );
        cor.join();
    }

    template< typename Coroutine, typename Next >
    void splitAccumulate( Coroutine &cor, int max, Next next ) {
        int sz = 0, ch = 0, total = 0;

        if ( max <= 32 )
            return cor.consume( max );

        Chunks leaves;

        while ( total < max ) {
            while ( ch < 32 && total < max ) {
                sz = next();
                total += sz;
                ch += sz;
            }
            leaves.emplace_back( ChunkT::NA, ch );
            ch = 0;
        }

        ASSERT_EQ( total, max );

        Levels levels;
        int width = leaves.size();
        while ( width > 1 ) {
            int div = width > 8 ? 4 : 2;
            width /= div;
            levels.push_front( div );
        }

        splitAccumulate( cor, leaves, levels, 0, leaves.size(), 0 );
    }

    template< typename Coroutine >
    void splitHint( Coroutine &cor, Chunk ch ) {
        if ( LS != LLVMSplitter::PerObject )
            return Common::splitHint( cor, ch.second, 0 );

        size_t i = 0, size = 0;
        auto &info = *bitcode->info;

        switch ( ch.first ) {
            case ChunkT::Flags:
                cor.consume( ch.second ); break;
            case ChunkT::Globals:
                cor.split( 2 );
                splitAccumulate( cor, info.globalsize, [&]() {
                        unsigned s;
                        if ( i < info.globalvars.size() ) {
                            s = info.globalvars[ i++ ].second.width;
                            size += s;
                        } else {
                            s = info.globalsize - size;
                            ASSERT_LEQ( info.globalsize - size, 3 );
                        }
                        return s; } );
                Common::splitHint( cor, ch.second - info.globalsize, 0 );
                cor.join();
                break;
            case ChunkT::Threads: {
                auto thr = state( cor.item ).sub( Threads() );
                int count = thr.get().length();
                int extra = sizeof( thr.get().length() );
                if ( count > 1 )
                    cor.split( count );
                for ( int i = 0; i < count; ++i ) {
                    Common::splitHint( cor, extra + thr.sub( i ).size(), 0 );
                    extra = 0;
                }
                if ( count > 1 )
                    cor.join();
                break;
            }
            default:
                return Common::splitHint( cor, ch.second, 0 );
        }
    }

    template< typename Coroutine >
    void splitHint( Coroutine &cor ) {
        auto state = this->state( cor.item );

        /* all these include slack in their offsets */
        Chunks starts;
        if ( LS == LLVMSplitter::Generic )
            starts = Chunks{ Chunk( ChunkT::NA, state.address( Flags() ).offset ) };
        else
            starts = Chunks { Chunk( ChunkT::Flags, state.address( Flags() ).offset ),
                              Chunk( ChunkT::Globals, state.address( machine::Globals() ).offset ),
                              Chunk( ChunkT::Heap, state.address( Heap() ).offset ),
                              Chunk( ChunkT::HeapM, state.address( HeapMeta() ).offset ),
                              Chunk( ChunkT::Threads, state.address( Threads() ).offset ) };

        Chunks ends = starts;

        ends.pop_front();
        ends.push_back( Chunk( ChunkT::NA, pool().size( cor.item ) ) );
        Chunks chunks;

        auto end = ends.begin();
        for ( auto ch : starts ) {
            if ( end->second - ch.second > 0 )
                chunks.emplace_back( ch.first, end->second - ch.second );
            ++end;
        }

        cor.split( chunks.size() );

        for ( auto ch : chunks )
            splitHint( cor, ch );

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
            s += "LTL: " + brick::string::fmt( buchi ) + " (";
            for ( int i = 0; i < int( prop_next[ buchi ].size() ); ++i ) {
                int next = prop_next[buchi][i];
                s += brick::string::fmt( prop_trans[next].first ) + " -> " +
                     brick::string::fmt( prop_trans[next].second ) + "; ";
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
        return apNames[ lit ] + 1;
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
                ASSERT_EQ( s.size(), 1u );
                return;
            }

            if ( useProperty( name ) )
                continue;

            if ( props.count( name ) )
                ltl += (ltl.empty() ? "" : " && ") + props[ name ];
            else
                throw std::logic_error(
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

        ASSERT_EQ( initial.size(), 1U );
        prop_initial = initial.front()->name;
        prop_next.resize( nodes.size() );
        prop_accept.resize( nodes.size(), false );

        for ( NodeList::iterator n = nodes.begin(); n != nodes.end(); ++n ) {
            ASSERT_LEQ( (*n)->name, int( nodes.size() ) );
            int nid = (*n)->name - 1;

            for ( TransList::const_iterator t = b.get_node_adj(*n).begin();
                  t != b.get_node_adj(*n).end(); ++t ) {
                std::vector< int > guard;

                for ( LTL_label_t::const_iterator l = t->t_label.begin();
                      l != t->t_label.end(); ++l ) {
                    ASSERT_LEQ( 1, literal_id( l->predikat ) );
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

        apNames = _interpreter->describeAPs();

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
