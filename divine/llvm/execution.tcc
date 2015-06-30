// -*- C++ -*- (c) 2012 Petr Rockai

#include <divine/llvm/interpreter.h>
#include <divine/llvm/execution.h>

using namespace divine::llvm;

template< typename HM, typename L >
using Eval = Evaluator< Interpreter< HM, L >, Interpreter< HM, L > >;

template< typename HM, typename L >
void Interpreter< HM, L >::choose( int32_t result )
{
    ASSERT_EQ( instruction().builtin, BuiltinChoice );
    *reinterpret_cast< int32_t * >(
        dereference( instruction().result() ) ) = result;
    auto flags = memoryflag( instruction().result() );
    for ( int i = 0; i < int( sizeof( int32_t ) ); ++i, ++flags )
        flags.set( MemoryFlag::Data );
}

template< typename HM, typename L >
void Interpreter< HM, L >::evaluate()
{
    Eval< HM, L > eval( info(), *this, *this );
    eval._instruction = &instruction();
    eval.run();
}

template< typename HM, typename L >
void Interpreter< HM, L >::evaluateSwitchBB( PC to )
{
    Eval< HM, L > eval( info(), *this, *this );
    eval._instruction = &instruction();
    eval.switchBB( to );
}
