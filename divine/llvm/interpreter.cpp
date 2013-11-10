// -*- C++ -*- (c) 2011, 2012 Petr Rockai <me@mornfall.net>

#define NO_RTTI

#include <wibble/exception.h>
#include <divine/llvm/interpreter.h>

#include <divine/llvm/wrap/DerivedTypes.h>
#include <divine/llvm/wrap/Constants.h>
#include <divine/llvm/wrap/Module.h>
#include <divine/llvm/wrap/LLVMContext.h>
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

    for ( int i = 0; i < int( info().constinfo.size() ); ++i ) {
        auto v = info().constinfo[ i ];
        auto id = v.second;

        if ( std::string( id, 0, strlen( prefix ) ) != prefix )
            continue;

        std::string name( id, strlen( prefix ), std::string::npos );
        GlobalContext ctx( info(), TD, nullptr );
        int idx = i;
        while ( true ) {
            auto val = info().globals[ idx ];
            if ( val.type != ProgramInfo::Value::Pointer )
                break;
            auto valptr = *reinterpret_cast< Pointer * >( ctx.dereference( val ) );
            idx = valptr.segment;
        }
        auto str = info().globals[ idx ];
        properties[ name ] = ctx.dereference( str );
    }
}

divine::Blob Interpreter::initial( Function *f )
{
    Blob pre_initial = alloc.makeBlobCleared( state.size( 0, 0, 0, 0 ) );
    state.rewind( pre_initial, 0 ); // there isn't a thread really
    std::copy( info().globaldata.begin(), info().globaldata.end(), state.global().memory() );
    auto fl = state.global().memoryflag( info() );
    for ( int i = 0; i < info().globaldata.size(); ++ i ) {
        fl.set( MemoryFlag::Data );
        ++ fl;
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

int Interpreter::new_thread( PC pc, Maybe< Pointer > arg, MemoryFlag fl )
{
    int current = state._thread;
    int tid = state.new_thread();
    state.enter( pc.function );
    if ( !arg.nothing() ) {
        auto v = info().function( pc ).values[ 0 ];
        frame().memoryflag( info(), v ).set( fl );
        *frame().dereference< Pointer >( info(), v ) = arg.value();
    }
    if ( current >= 0 )
        state.switch_thread( current );
    return tid;
}

int Interpreter::new_thread( Function *f )
{
    return new_thread( PC( info().functionmap[ f ], 0 ),
                       Maybe< Pointer >::Nothing(), MemoryFlag::Data );
}

