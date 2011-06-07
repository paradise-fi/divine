// -*- C++ -*- (c) 2007, 2008, 2009 Petr Rockai <me@mornfall.net>

#ifdef HAVE_LLVM
#include <stdint.h>
#include <iostream>

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Debug.h>
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

        divine::llvm::Interpreter &interpreter() const {
            assert( _parent );
            return _parent->interpreter();
        }

        bool empty() const {
            if (!_from.valid())
                return true;
            interpreter().restore( _from, _parent->alloc._slack );
            return interpreter().done() || !interpreter().alternatives( _alternative );
        }

        Node from() { return _from; }

        Successors tail() const {
            Successors s = *this;
            ++ s._alternative;
            return s;
        }

        Node head() {
            interpreter().restore( _from, _parent->alloc._slack );
            interpreter().step( _alternative );
            return interpreter().snapshot( _parent->alloc._slack, _parent->pool() );
        }

        Successors() {}
    };

    Successors successors( Node st ) {
        Successors ret;
        ret._from = st;
        ret._parent = this;
        ret._alternative = 0;
        return ret;
    }

    void release( Node s ) {
        s.free( pool() );
    }

    bool isGoal( Node s ) {
        return false;
    }

    bool isAccepting( Node s ) { return false; }

    std::string showNode( Node s ) {
        return "[?]";
    }

    void die( std::string err ) __attribute__((noreturn)) {
        std::cerr << err << std::endl;
        exit( 1 );
    }

    void read( std::string _file ) {
        file = _file;
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
