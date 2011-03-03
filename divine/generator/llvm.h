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

struct LLVM : Common {
    typedef Blob Node;
    divine::llvm::Interpreter *_interpreter;
    OwningPtr< MemoryBuffer > *_input;
    LLVMContext *ctx;
    std::string file;
    Node _initial;

    Node initial() {
        interpreter();
        dbgs() << "initial: " << interpreter().nextInstruction() << "\n";
        assert( _initial.valid() );
        return _initial;
    }

    template< typename Q >
    void queueInitials( Q &q ) {
        q.queue( Node(), initial() );
    }

    struct Successors {
        typedef Node Type;
        Node _from, _this;
        bool _empty;

        bool empty() const {
            return _empty;
        }

        Node from() { return _from; }

        Successors tail() const {
            Successors s = *this;
            s._empty = true;
            return s;
        }

        Node head() {
            return _this;
        }

        Successors() : _empty( true ) {}
    };

    Successors successors( Node st ) {
        Successors ret;
        interpreter().restore( st, alloc._slack );
        dbgs() << "successors: " << interpreter().nextInstruction() << "; tid = "
               << pthread_self() << "\n";

        ret._from = st;
        if (interpreter().done()) {
            dbgs() << "successors: done\n";
            ret._empty = true;
        } else {
            ret._empty = false;
            interpreter().step();
            ret._this = interpreter().snapshot( alloc._slack, pool() );
            assert( ret._this.valid() );
        }
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
        std::cerr << "loaded LLVM bitcode file " << file << "..." << std::endl;

        _initial = _interpreter->snapshot( alloc._slack, pool() );
        return *_interpreter;
    }

    LLVM() : _interpreter( 0 ) {}

};

}
}

#endif
#endif
