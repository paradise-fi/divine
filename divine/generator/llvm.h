// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#ifdef HAVE_LLVM
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
#include <divine/llvm/interpreter.h>
#include <divine/buchi.h>

#ifndef DIVINE_GENERATOR_LLVM_H
#define DIVINE_GENERATOR_LLVM_H

namespace divine {
namespace generator {

using namespace llvm;

struct LLVM : Common< Blob > {
    typedef Blob Node;
    divine::llvm::Interpreter *_interpreter;
    Module *_module;
    OwningPtr< MemoryBuffer > *_input;
    LLVMContext *ctx;
    std::string file;
    Node _initial;

    typedef std::vector< int > PropGuard;
    typedef std::pair< PropGuard, int > PropTrans;

    std::vector< std::vector< int > > prop_next;
    std::vector< PropTrans > prop_trans;
    std::vector< bool > prop_accept;
    int prop_initial;

    bool use_property;

    Node initial() {
        interpreter();
        assert( _initial.valid() );
        return _initial;
    }

    template< typename Q >
    void queueInitials( Q &q ) {
        q.queue( Node(), initial() );
    }

    struct Successors {
        typedef Node Type;
        LLVM *_parent;
        Node _from;
        int _alternative;
        int _context;

        divine::llvm::Interpreter &interpreter() const {
            assert( _parent );
            return _parent->interpreter();
        }

        bool empty() const {
            if (!_from.valid())
                return true;
            interpreter().restore( _from, _parent->alloc._slack );
            bool empty = !interpreter().viable( _context, _alternative )
                         && !interpreter().viable( _context + 1, 0 );
            return empty;
        }

        Node from() { return _from; }

        Successors tail() const {
            Successors s = *this;
            interpreter().restore( _from, _parent->alloc._slack );
            if ( interpreter().viable( _context, _alternative + 1 ) ) {
                ++ s._alternative;
            } else {
                ++ s._context;
                s._alternative = 0;
            }
            return s;
        }

        Node head() {
            interpreter().restore( _from, _parent->alloc._slack );
            interpreter().step( _context, _alternative );

            // collapse all following tau actions in this context
            while ( interpreter().isTau( _context ) )
                interpreter().step( _context, 0 ); // tau steps cannot branch

            return interpreter().snapshot( _parent->alloc._slack, _parent->pool() );
        }

        Successors() {}
    };

    Successors successors( Node st ) {
        Successors ret;
        ret._from = st;
        ret._parent = this;
        ret._alternative = 0;
        ret._context = 0;
        return ret;
    }

    void release( Node s ) {
        s.free( pool() );
    }

    bool isGoal( Node n ) {
        interpreter().restore( n, alloc._slack );
        return interpreter().flags.assert
            || interpreter().flags.null_dereference
            || interpreter().flags.invalid_dereference;
    }

    bool isAccepting( Node s ) { return false; }

    std::string showNode( Node n ) {
        interpreter().restore( n, alloc._slack );
        return interpreter().describe();
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
        assert( _module );
        NamedMDNode *enums = _module->getNamedMetadata("llvm.dbg.enum");
        assert( enums );
        for ( int i = 0; i < enums->getNumOperands(); ++i ) {
            MDNode *n = cast< MDNode >( enums->getOperand(i) );
            n = cast< MDNode >( n->getOperand(10) ); // the list of enum items
            for ( int j = 0; j < n->getNumOperands(); ++j ) {
                MDNode *it = cast< MDNode >( n->getOperand(j) );
                MDString *name = cast< MDString >( it->getOperand(1) );
                if ( name->getString() == lit )
                    return 1 + interpreter().constant(
                        cast< Constant >( it->getOperand(2) ) ).IntVal.getZExtValue();
            }
        }
        assert_die();
    }

    void useProperty( std::string name ) {
        std::string ltl = interpreter().properties[ name ];
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
                      l != t->t_label.end(); ++l )
                    guard.push_back( (-1 * l->negace) * literal_id( l->predikat ) );
                prop_next[nid].push_back( prop_trans.size() );
                prop_trans.push_back( std::make_pair( guard, t->target->name - 1 ) );
            }
        }
        for ( NodeList::iterator n = accept.begin(); n != accept.end(); ++n )
            prop_accept[(*n)->name - 1] = true;
    }

    divine::llvm::Interpreter &interpreter() {
        if (_interpreter)
            return *_interpreter;

        _input = new OwningPtr< MemoryBuffer >();
        ctx = new LLVMContext();
        MemoryBuffer::getFile( file, *_input );
        Module *m = ParseBitcodeFile( &**_input, *ctx );

        assert( m );
        std::string err;

        _interpreter = new Interpreter( m );
        _module = m;

        std::vector<llvm::GenericValue> args;

        Function *f = m->getFunction( "main" );
        assert( f );
        _interpreter->callFunction( f, args );
        _initial = _interpreter->snapshot( alloc._slack, pool() );
        return *_interpreter;
    }

    LLVM() : _interpreter( 0 ), use_property( false ) {}

};

}
}

#endif
#endif
