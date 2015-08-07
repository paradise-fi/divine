// -*- C++ -*- (c) 2011, 2012 Petr Rockai <me@mornfall.net>

#include <divine/llvm/interpreter.h>

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
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
        GlobalContext ctx( info(), TD, false );
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
int Interpreter< HM, L >::new_thread( PC pc, Maybe< Pointer > arg, MemoryFlag fl )
{
    int current = state._thread;
    int tid = state.new_thread();
    state.enter( pc.function );
    if ( !arg.isNothing() ) {
        auto v = info().function( pc ).values[ 0 ];
        auto mflag = frame().memoryflag( info(), v );
        mflag.set( fl ); ++ mflag;
        if ( fl == MemoryFlag::HeapPointer )
            fl = MemoryFlag::Data;
        for ( int i = 1; i < v.width; ++i, ++mflag )
            mflag.set( fl );
        *frame().template dereference< Pointer >( info(), v ) = arg.value();
    }
    if ( current >= 0 )
        state.switch_thread( current );
    observed = true;
    return tid;
}

template< typename HM, typename L >
int Interpreter< HM, L >::new_thread( Function *f )
{
    return new_thread( PC( info().functionmap[ f ], 0 ),
                       Maybe< Pointer >::Nothing(), MemoryFlag::Data );
}

template< typename HM, typename L >
brick::data::Bimap< int, std::string > Interpreter< HM, L >::describeAPs() {
    MDNode *apmeta = findEnum( "APs" );
    if ( !apmeta )
        return { };

    brick::data::Bimap< int, std::string > out;
    for ( int i = 0, end = apmeta->getNumOperands(); i < end; ++i ) {
        auto ap = cast< MDNode >( apmeta->getOperand( i ) );
        auto ap_val = cast< llvm::ValueAsMetadata >( ap->getOperand( 2 ) );
        auto ap_int = cast< ConstantInt >( ap_val->getValue() )->getValue().getZExtValue();
        out.insert( ap_int, cast< MDString >( ap->getOperand( 1 ) )->getString() );
    }
    return out;
}
