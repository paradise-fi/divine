//===- Interpreter.cpp - Top-Level LLVM Interpreter Implementation --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the top-level functionality for the LLVM interpreter.
// This interpreter is designed to be a very simple, portable, inefficient
// interpreter.
//
//===----------------------------------------------------------------------===//

#define NO_RTTI
#include <wibble/exception.h>
#include <divine/llvm/interpreter.h>
#include "llvm/CodeGen/IntrinsicLowering.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Constants.h"
#include "llvm/Module.h"
#include <cstring>
using namespace llvm;
using namespace divine::llvm;

Interpreter::Interpreter(Allocator &alloc, Module *M)
    : ExecutionEngine(M), TD(M), module( M ), info( TD, *this ), alloc( alloc ), state( info, alloc )
{
    setTargetData(&TD);
    initializeExecutionEngine();
    IL = new IntrinsicLowering(TD);
    parseProperties( M );
    info.build( M );
}

Interpreter::~Interpreter() {
  delete IL;
}

void Interpreter::parseProperties( Module *M )
{
    for (Module::const_global_iterator var = M->global_begin(); var != M->global_end(); ++var)
    {
        if ( var->isConstant() )
            continue;

        // GlobalVariable type is always a pointer, so dereference it first
        Type *ty = var->getType()->getTypeAtIndex(unsigned(0));
        assert( ty );

        assert( !var->isDeclaration() ); // can't handle extern's yet
        if ( std::string( var->getName().str(), 0, 6 ) == "__LTL_" ) {
            std::string name( var->getName().str(), 6, std::string::npos );
            GenericValue GV = getConstantValue(var->getInitializer());
            properties[name] = std::string( (char*) GV.PointerVal );
            continue; // do not emit this one
        }
    }
}

divine::Blob Interpreter::initial( Function *f )
{
    Blob pre_initial = alloc.new_blob( state.size( 0, 0, 0 ) );
    pre_initial.clear();
    state.rewind( pre_initial, 0 ); // there isn't a thread really

    int idx = 0;
    for ( auto var = module->global_begin(); var != module->global_end(); ++ var, ++ idx ) {
        auto val = info.globals[ idx ];
        if ( var->hasInitializer() )
            info.storeGV( state.dereference( val ),
                          getConstantValue( var->getInitializer() ),
                          var->getType(), val.width );
    }

    int tid = state.new_thread(); // switches automagically
    assert_eq( tid, 0 ); // just to be on the safe side...
    state.enter( info.functionmap[ f ] );
    Blob result = state.snapshot();
    state.rewind( result, 0 ); // so that we don't wind up in an invalid state...
    pre_initial.free( alloc.pool() );
    return result;
}

void Interpreter::new_thread( PC pc )
{
    int current = state._thread;
    int tid = state.new_thread();
    state.enter( pc.function );
    state.switch_thread( current );
}

void Interpreter::new_thread( Function *f )
{
    new_thread( PC( info.functionmap[ f ], 0, 0 ) );
}
