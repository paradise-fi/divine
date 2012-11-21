// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#ifdef O_LLVM
#include <stdint.h>
#include <iostream>

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/ADT/OwningPtr.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/Support/system_error.h>

#include <divine/generator/common.h>
#include <divine/llvm-new/interpreter.h>
#include <divine/ltl2ba/main.h>

#ifndef DIVINE_GENERATOR_LLVM_H
#define DIVINE_GENERATOR_LLVM_H

namespace divine {
namespace generator {

using namespace ::llvm;

struct LLVM : Common< Blob > {
    typedef Blob Node;
    divine::llvm2::Interpreter *_interpreter, *_interpreter_2;
    Module *_module;
    OwningPtr< MemoryBuffer > *_input;
    LLVMContext *ctx;
    std::string file;
    Node _initial;
    typedef wibble::Unit Label;

    typedef std::vector< int > PropGuard;
    typedef std::pair< PropGuard, int > PropTrans;

    std::vector< std::vector< int > > prop_next;
    std::vector< PropTrans > prop_trans;
    std::vector< bool > prop_accept;
    int prop_initial;

    bool use_property;

    Node initial() {
        assert( _module );
        Function *f = _module->getFunction( "main" );
        assert( f );
        return interpreter().initial( f );
    }

    template< typename Q >
    void queueInitials( Q &q ) {
        q.queue( Node(), initial(), Label() );
    }

    template< typename Yield >
    void successors( Node st, Yield yield ) {
        interpreter().run( st, [&]( Node n ){ yield( n, Label() ); } );
    }

    void release( Node s ) {
        s.free( pool() );
    }

    divine::llvm2::MachineState::Flags &flags( Blob b ) {
        return b.get< divine::llvm2::MachineState::Flags >( alloc._slack );
    }

    bool isGoal( Node n ) {
        auto fl = flags( n );
        return fl.assert || fl.null_dereference || fl.invalid_dereference || fl.invalid_argument;
    }

    bool isAccepting( Node n ) {
        if ( !use_property )
            return false;
        return prop_accept[ flags( n ).buchi ];
    }

    std::string showNode( Node n ) {
        interpreter(); /* ensure _interpreter_2 is initialised */
        _interpreter_2->rewind( n );
        std::string s = _interpreter_2->describe();
        if ( use_property ) {
            int buchi = _interpreter_2->state.flags().buchi;
            s += "\nLTL: " + wibble::str::fmt( buchi ) + " (";
            for ( int i = 0; i < prop_next[ buchi ].size(); ++i ) {
                int next = prop_next[buchi][i];
                s += wibble::str::fmt( prop_trans[next].first ) + " -> " +
                     wibble::str::fmt( prop_trans[next].second ) + "; ";
            }
            s += ")";
        }
        return s;
    }

    std::string showTransition( Node from, Node to, Label ) {
        return ""; // dummy
    }

    void die( std::string err ) __attribute__((noreturn)) {
        std::cerr << err << std::endl;
        exit( 1 );
    }

    void read( std::string _file ) {
        file = _file;
    }

    template< typename O >
    O getProperties( O o ) {
        return std::copy( interpreter().properties.begin(),
                          interpreter().properties.end(),
                          o );
    }

    int literal_id( std::string lit ) {
        MDNode *ap = interpreter().findEnum( "AP" );
        assert( ap );
        for ( int i = 0; i < ap->getNumOperands(); ++i ) {
            MDNode *it = cast< MDNode >( ap->getOperand(i) );
            MDString *name = cast< MDString >( it->getOperand(1) );
            if ( name->getString() == lit )
                return 1 + interpreter().constantGV(
                    cast< Constant >( it->getOperand(2) ) ).IntVal.getZExtValue();
        }
        assert_die();
    }

    void useProperty( meta::Input &i ) {
        std::string name = i.propertyName, ltl;

        if ( interpreter().properties.count( name ) )
            ltl = interpreter().properties[ name ];
        if ( ltl.empty() )
            return;

        i.property = ltl;
        i.propertyType = meta::Input::LTL;

        use_property = true;
        BA_opt_graph_t b = buchi( ltl );

        typedef std::list< KS_BA_node_t * > NodeList;
        typedef std::list< BA_trans_t > TransList;
        std::set< std::string > lits;

        NodeList nodes, initial, accept;
        nodes = b.get_all_nodes();
        initial = b.get_init_nodes();
        accept = b.get_accept_nodes();

        assert_eq( initial.size(), 1 );
        prop_initial = initial.front()->name;
        prop_next.resize( nodes.size() );
        prop_accept.resize( nodes.size(), false );

        for ( NodeList::iterator n = nodes.begin(); n != nodes.end(); ++n ) {
            assert_leq( (*n)->name, nodes.size() );
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

    divine::llvm2::Interpreter &interpreter() {
        if (_interpreter)
            return *_interpreter;

        _input = new OwningPtr< MemoryBuffer >();
        ctx = new LLVMContext();
        MemoryBuffer::getFile( file, *_input );
        Module *m = ParseBitcodeFile( &**_input, *ctx );

        assert( m );
        std::string err;

        _interpreter = new divine::llvm2::Interpreter( alloc, m );
        _interpreter_2 = new divine::llvm2::Interpreter( alloc, m );
        _module = m;

        return *_interpreter;
    }

    LLVM() : _interpreter( 0 ), use_property( false ) {}
    LLVM( const LLVM &other ) {
        *this = other;
        _interpreter = 0;
        _initial = Node();
    }

};

}
}

#endif
#endif
