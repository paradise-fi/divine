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

#ifdef HAVE_LLVM

#include <divine/llvm/interpreter.h>
#include "llvm/CodeGen/IntrinsicLowering.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Module.h"
#include <cstring>
using namespace llvm;
using namespace divine::llvm;

//===----------------------------------------------------------------------===//
// Interpreter ctor - Initialize stuff
//
Interpreter::Interpreter(Module *M)
  : ExecutionEngine(M), TD(M) {
      
  memset(&ExitValue.Untyped, 0, sizeof(ExitValue.Untyped));
  setTargetData(&TD);
  // Initialize the "backend"
  initializeExecutionEngine();
  // initializeExternalFunctions();
  emitGlobals();

  buildIndex( M );
  _alternative = 0;

  IL = new IntrinsicLowering(TD);
}

Interpreter::~Interpreter() {
  delete IL;
}

void Interpreter::runAtExitHandlers () {
  while (!AtExitHandlers.empty()) {
    callFunction(AtExitHandlers.back(), std::vector<GenericValue>());
    AtExitHandlers.pop_back();
    run();
  }
}

/// run - Start execution with the specified function and arguments.
///
GenericValue
Interpreter::runFunction(Function *F,
                         const std::vector<GenericValue> &ArgValues) {
  assert (F && "Function *F was null at entry to run()");

  // Try extra hard not to pass extra args to a function that isn't
  // expecting them.  C programmers frequently bend the rules and
  // declare main() with fewer parameters than it actually gets
  // passed, and the interpreter barfs if you pass a function more
  // parameters than it is declared to take. This does not attempt to
  // take into account gratuitous differences in declared types,
  // though.
  std::vector<GenericValue> ActualArgs;
  const unsigned ArgCount = F->getFunctionType()->getNumParams();
  for (unsigned i = 0; i < ArgCount; ++i)
    ActualArgs.push_back(ArgValues[i]);

  // Set up the function call.
  callFunction(F, ActualArgs);

  // Start executing the function.
  run();

  return ExitValue;
}

void Interpreter::buildIndex( Module *module ) {
    int i = 0;
    for ( Module::iterator function = module->begin(); function != module->end(); ++function ) {
        for ( Function::iterator block = function->begin(); block != function->end(); ++block ) {
            BasicBlock::iterator insn;
            for ( insn = block->begin(); insn != block->end(); ++insn ) {
                locationIndex.insert( i++, Location( function, block, insn ) );
                valueIndex.insert( i, &*insn );
            }
            /* we also insert the end() iterator, because it is intermittently
             * stored at the end of basic blocks */
            locationIndex.insert( i++, Location( function, block, insn ) );
            for ( Function::arg_iterator arg = function->arg_begin();
                  arg != function->arg_end(); ++arg )
                valueIndex.insert( i++, arg );
        }
    }
}

Location Interpreter::location( ExecutionContext &SF ) {
    return locationIndex.right( SF.pc );
}

CallSite Interpreter::caller( ExecutionContext &SF ) {
    assert_leq( 0, SF.caller );
    return CallSite( locationIndex.right( SF.caller ).insn );
}

void Interpreter::setLocation( ExecutionContext &SF, Location l ) {
    SF.pc = locationIndex.left( l );
}

namespace divine { namespace llvm {
std::ostream &operator<<( std::ostream &ostr, Location l ) {
    return ostr << "<" << l.function->getNameStr() << ", " << l.insn->getNameStr() << ">";
}
} }

#endif
