// -*- C++ -*- (c) 2011, 2012 Petr Rockai <me@mornfall.net>

#define NO_RTTI

#include <wibble/exception.h>
#include <divine/llvm/interpreter.h>

#include "llvm/DerivedTypes.h"
#include "llvm/Constants.h"
#include "llvm/Module.h"
#include "llvm/LLVMContext.h"
#include <cstring>

using namespace llvm;
using namespace divine::llvm;

Interpreter::Interpreter(Allocator &alloc, Module *M)
    : TD( M ), module( M ), info( M ), alloc( alloc ), state( info, alloc )
{
    tauplus = false;
    taustores = false;
    tauminus = false;
    parseProperties( M );
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
            assert_die();
            // GenericValue GV = getConstantValue(var->getInitializer());
            // properties[name] = std::string( (char*) GV.PointerVal );
            continue; // do not emit this one
        }
    }
}

divine::Blob Interpreter::initial( Function *f )
{
    Blob pre_initial = alloc.new_blob( state.size( 0, 0, 0, 0 ) );
    pre_initial.clear();
    state.rewind( pre_initial, 0 ); // there isn't a thread really

    for ( auto var = module->global_begin(); var != module->global_end(); ++ var ) {
        auto val = info.valuemap[ &*var ];
        if ( !var->isConstant() && var->hasInitializer() ) {
            assert( val.constant ); /* the pointer */
            Pointer p = *reinterpret_cast< Pointer * >( dereference( val ) );
            info.storeConstant( info.globals[ p.segment ], var->getInitializer(),
                                reinterpret_cast< char * >( state.global().memory ) );
        }
    }

    int tid = state.new_thread(); // switches automagically
    assert_eq( tid, 0 ); // just to be on the safe side...
    state.enter( info.functionmap[ f ] );
    Blob result = state.snapshot();
    state.rewind( result, 0 ); // so that we don't wind up in an invalid state...
    pre_initial.free( alloc.pool() );
    return result;
}

int Interpreter::new_thread( PC pc, Maybe< Pointer > arg, bool ptr )
{
    int current = state._thread;
    int tid = state.new_thread();
    state.enter( pc.function );
    if ( !arg.nothing() ) {
        auto v = info.function( pc ).values[ 0 ];
        frame().setPointer( info, v, ptr );
        *frame().dereference< Pointer >( info, v ) = arg.value();
    }
    if ( current >= 0 )
        state.switch_thread( current );
    return tid;
}

int Interpreter::new_thread( Function *f )
{
    return new_thread( PC( info.functionmap[ f ], 0, 0 ), Maybe< Pointer >::Nothing() );
}

