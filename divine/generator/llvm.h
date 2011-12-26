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

#ifndef DIVINE_GENERATOR_LLVM_H
#define DIVINE_GENERATOR_LLVM_H

namespace divine {
namespace generator {

using namespace llvm;

struct LLVM : Common< Blob > {
    typedef Blob Node;
    divine::llvm::Interpreter *_interpreter;
    OwningPtr< MemoryBuffer > *_input;
    LLVMContext *ctx;
    std::string file;
    Node _initial;

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

        std::vector<llvm::GenericValue> args;

        Function *f = m->getFunction( "main" );
        assert( f );
        _interpreter->callFunction( f, args );
        _initial = _interpreter->snapshot( alloc._slack, pool() );
        return *_interpreter;
    }

    LLVM() : _interpreter( 0 ) {}

};

}
}

#endif
#endif
