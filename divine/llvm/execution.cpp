// -*- C++ -*- (c) 2012 Petr Rockai

#include <divine/llvm/interpreter.h>
#include <divine/llvm/execution.h>

using namespace divine::llvm;

typedef Evaluator< Interpreter, Interpreter > Eval;

void Interpreter::choose( int32_t result )
{
    assert_eq( instruction().builtin, BuiltinChoice );
    *reinterpret_cast< int32_t * >(
        dereference( instruction().result() ) ) = result;
}

void Interpreter::evaluate()
{
    Eval eval( info, *this, *this );
    eval.instruction = instruction();
    eval.run();
    eval.check();
}

void Interpreter::evaluateSwitchBB( PC to )
{
    Eval eval( info, *this, *this );
    eval.instruction = instruction();
    eval.switchBB( to );
}
