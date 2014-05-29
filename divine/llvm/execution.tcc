// -*- C++ -*- (c) 2012 Petr Rockai

#include <divine/llvm/interpreter.h>
#include <divine/llvm/execution.h>

using namespace divine::llvm;

template< typename HM >
using Eval = Evaluator< Interpreter< HM >, Interpreter< HM > >;

template< typename HM >
void Interpreter< HM >::choose( int32_t result )
{
    assert_eq( instruction().builtin, BuiltinChoice );
    *reinterpret_cast< int32_t * >(
        dereference( instruction().result() ) ) = result;
}

template< typename HM >
void Interpreter< HM >::evaluate()
{
    Eval< HM > eval( info(), *this, *this );
    eval.instruction = instruction();
    eval.run();
}

template< typename HM >
void Interpreter< HM >::evaluateSwitchBB( PC to )
{
    Eval< HM > eval( info(), *this, *this );
    eval.instruction = instruction();
    eval.switchBB( to );
}
