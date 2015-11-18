// -*- C++ -*- (c) 2012 Petr Rockai

#include <divine/llvm/interpreter.hpp>
#include <divine/llvm/execution.hpp>

using namespace divine::llvm;

template< typename HM, typename L >
using Eval = Evaluator< Interpreter< HM, L >, Interpreter< HM, L > >;

template< typename HM, typename L >
void Interpreter< HM, L >::choose( int32_t result )
{
    ASSERT_EQ( instruction().builtin, BuiltinChoice );
    MemoryAccess acc( instruction().result(), *this );
    acc.setAndShift< int32_t >( result );
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
