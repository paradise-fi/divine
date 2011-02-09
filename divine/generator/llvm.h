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
#include <llvm/ExecutionEngine/Interpreter.h>
#include <llvm/Support/system_error.h>

#include <divine/generator/common.h>

#ifndef DIVINE_GENERATOR_LLVM_H
#define DIVINE_GENERATOR_LLVM_H

namespace divine {
namespace generator {

using namespace llvm;

struct LLVM : Common {
    typedef Blob Node;

    Node initial() {
        assert_die();
        /* Blob b = alloc.new_blob( sizeof( Content ) );
        b.get< Content >( alloc._slack ) = std::make_pair( 0, 0 );
        return b; */
    }

    template< typename Q >
    void queueInitials( Q &q ) {
        q.queue( Node(), initial() );
    }

    struct Successors {
        typedef Node Type;
        mutable Node _from;
        int nth;
        LLVM *parent;

        bool empty() const {
            /* if ( !_from.valid() )
                return true;
            Content f = _from.get< Content >( parent->alloc._slack );
            if ( f.first == 1024 || f.second == 1024 )
            return true; */
            return true;
        }

        Node from() { return _from; }

        Successors tail() const {
            Successors s = *this;
            s.nth ++;
            return s;
        }

        Node head() {
            assert_die();
            /* Node ret = parent->alloc.new_blob( sizeof( Content ) );
            ret.get< Content >( parent->alloc._slack ) =
                _from.get< Content >( parent->alloc._slack );
                return ret; */
        }
    };

    Successors successors( Node st ) {
        Successors ret;
        ret._from = st;
        ret.nth = 1;
        ret.parent = this;
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
        return "[]";
        /* if ( !s.valid() )
           return "[]";
        Content f = s.get< Content >( alloc._slack );
        std::stringstream stream;
        stream << "[" << f.first << ", " << f.second << "]";
        return stream.str(); */
    }

    void die( std::string err ) __attribute__((noreturn)) {
        std::cerr << err << std::endl;
        exit( 1 );
    }

    void read( std::string file ) {
        LLVMContext ctx;
        OwningPtr< MemoryBuffer > input;
        MemoryBuffer::getFile( file, input );
        Module *m = ParseBitcodeFile( &*input, ctx );

        assert( m );
        std::string err;
        ExecutionEngine *engine = EngineBuilder( m ).setEngineKind( EngineKind::Interpreter )
                                  .setErrorStr( &err ).create();

        if ( !engine )
            die( err );

        Interpreter *inter = static_cast< Interpreter * >( engine );
        assert( inter );

        std::vector<llvm::GenericValue> args;

        Function *f = m->getFunction( "main" );
        assert( f );
        inter->callFunction( f, args );
        std::cerr << "loaded LLVM bitcode file " << file << "..." << std::endl;
    }

};

}
}

#endif
#endif
