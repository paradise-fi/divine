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

Interpreter::Interpreter( graph::Allocator &alloc, std::shared_ptr< BitCode > bc )
    : alloc( alloc ), bc( bc ), TD( bc->module.get() ), state( info(), alloc )
{
    tauplus = false;
    taustores = false;
    tauminus = false;
    parseProperties( bc->module.get() );
}

void Interpreter::parseProperties( Module *M )
{
    auto prefix = "__divine_LTL_";

    for ( auto v : info().constinfo ) {
        auto id = v.second.second;

        if ( std::string( id, 0, strlen( prefix ) ) != prefix )
            continue;

        std::string name( id, strlen( prefix ), std::string::npos );
        GlobalContext ctx( info(), TD, nullptr );
        auto val = info().globals[ v.first.segment ];
        auto valptr = *reinterpret_cast< Pointer * >( ctx.dereference( val ) );
        auto str = info().globals[ valptr.segment ];
        properties[ name ] = ctx.dereference( str );
    }
}

divine::Blob Interpreter::initial( Function *f )
{
    Blob pre_initial = alloc.makeBlobCleared( state.size( 0, 0, 0, 0 ) );
    state.rewind( pre_initial, 0 ); // there isn't a thread really

    for ( auto var = bc->module->global_begin(); var != bc->module->global_end(); ++ var ) {
        auto val = info().valuemap[ &*var ];
        if ( !var->isConstant() && var->hasInitializer() ) {
            assert( val.constant ); /* the pointer */
            Pointer p = *reinterpret_cast< Pointer * >( dereference( val ) );
            auto pointee = info().globals[ p.segment ];
            assert( !pointee.constant );
            info().storeConstant( pointee, var->getInitializer(),
                                  reinterpret_cast< char * >( state.global().memory() ) );
        }
    }

    int tid = state.new_thread(); // switches automagically
    assert_eq( tid, 0 ); // just to be on the safe side...
    static_cast< void >( tid );
    state.enter( info().functionmap[ f ] );
    Blob result = state.snapshot();
    state.rewind( result, 0 ); // so that we don't wind up in an invalid state...
    alloc.pool().free( pre_initial );
    return result;
}

int Interpreter::new_thread( PC pc, Maybe< Pointer > arg, bool ptr )
{
    int current = state._thread;
    int tid = state.new_thread();
    state.enter( pc.function );
    if ( !arg.nothing() ) {
        auto v = info().function( pc ).values[ 0 ];
        frame().setPointer( info(), v, ptr );
        *frame().dereference< Pointer >( info(), v ) = arg.value();
    }
    if ( current >= 0 )
        state.switch_thread( current );
    return tid;
}

int Interpreter::new_thread( Function *f )
{
    return new_thread( PC( info().functionmap[ f ], 0, 0 ), Maybe< Pointer >::Nothing() );
}

