// -*- C++ -*- (c) 2011, 2012 Petr Rockai <me@mornfall.net>

#define NO_RTTI

#include <divine/llvm/interpreter.h>

#include <divine/llvm/wrap/DerivedTypes.h>
#include <divine/llvm/wrap/Constants.h>
#include <divine/llvm/wrap/Module.h>
#include <divine/llvm/wrap/LLVMContext.h>
#include <cstring>

using namespace llvm;
using namespace divine::llvm;

template< typename HM, typename L >
Interpreter< HM, L >::Interpreter( Pool &pool, int slack, std::shared_ptr< BitCode > bc )
    : pool( pool ), bc( bc ), TD( bc->module.get() ), state( info(), pool, slack )
{
    tauplus = false;
    taustores = false;
    tauminus = false;
    parseProperties( bc->module.get() );
}

template< typename HM, typename L >
void Interpreter< HM, L >::parseProperties( Module * )
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

template< typename HM, typename L >
divine::Blob Interpreter< HM, L >::initial( Function *f, bool is_start )
{
    Blob pre_initial = pool.allocate( state._slack + state.size( 0, 0, 0, 0 ) );
    pool.clear( pre_initial );
    state.rewind( pre_initial, 0 ); // there isn't a thread really
    std::copy( info().globaldata.begin(), info().globaldata.end(), state.global().memory() );
    auto fl = state.global().memoryflag( info() );
    for ( int i = 0; i < int( info().globaldata.size() ); ++ i ) {
        fl.set( MemoryFlag::Data );
        ++ fl;
    }
    int tid = state.new_thread(); // switches automagically
    assert_eq( tid, 0 ); // just to be on the safe side...
    static_cast< void >( tid );
    state.enter( info().functionmap[ f ] );

    if ( is_start ) {
        auto &fun = info().function( PC( info().functionmap[ f ], 0 ) );
        auto ctors = info().module->getNamedGlobal( "llvm.global_ctors" );
        if ( ctors ) {
            auto ctor_arr = ::llvm::cast< ::llvm::ConstantArray >( ctors->getInitializer() );
            auto ctors_val = info().valuemap[ ctors ];

            assert_eq( fun.values[ 0 ].width, sizeof( int ) );
            assert_eq( fun.values[ 1 ].width, ctors_val.width );
            assert_eq( fun.values[ 2 ].width, sizeof( int ) );
            assert( info().module->getFunction( "main" ) );

            for ( int i = 0; i <= 2; ++i )
                state.memoryflag( fun.values[ i ] ).set( MemoryFlag::Data );

            *reinterpret_cast< int * >( state.dereference( fun.values[ 0 ] ) ) =
                ctor_arr->getNumOperands();
            memcopy( ctors_val, fun.values[ 1 ], ctors_val.width, state, state );
            *reinterpret_cast< int * >( state.dereference( fun.values[ 2 ] ) ) =
                info().module->getFunction( "main" )->arg_size();
        }
    }

    Blob result = state.snapshot();
    state.rewind( result, 0 ); // so that we don't wind up in an invalid state...
    pool.free( pre_initial );
    return result;
}

template< typename HM, typename L >
int Interpreter< HM, L >::new_thread( PC pc, Maybe< Pointer > arg, MemoryFlag fl )
{
    int current = state._thread;
    int tid = state.new_thread();
    state.enter( pc.function );
    if ( !arg.isNothing() ) {
        auto v = info().function( pc ).values[ 0 ];
        frame().memoryflag( info(), v ).set( fl );
        *frame().template dereference< Pointer >( info(), v ) = arg.value();
    }
    if ( current >= 0 )
        state.switch_thread( current );
    return tid;
}

template< typename HM, typename L >
int Interpreter< HM, L >::new_thread( Function *f )
{
    return new_thread( PC( info().functionmap[ f ], 0 ),
                       Maybe< Pointer >::Nothing(), MemoryFlag::Data );
}
