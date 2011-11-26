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

  initializeExecutionEngine();

  emitGlobals( M );
  buildIndex( M );

  stacks.resize( 1 );
  _context = 0;
  _alternative = 0;
  flags.assert = flags.null_dereference = flags.invalid_dereference = false;
  flags.ap = flags.buchi = 0;

  IL = new IntrinsicLowering(TD);
}

Interpreter::~Interpreter() {
  delete IL;
}

void Interpreter::emitGlobals( Module *M )
{
    for (Module::const_global_iterator var = M->global_begin(); var != M->global_end(); ++var) {
        if ( var->isConstant() )
            continue;
        assert( !var->isDeclaration() ); // can't handle extern's yet

        // GlobalVariable type is always a pointer, so dereference it first
        Type *ty = var->getType()->getTypeAtIndex(unsigned(0));
        assert( ty );
        globals.insert( std::make_pair( &*var, globalmem.size() + 0x100 ) );
        globalmem.resize( globalmem.size() + TD.getTypeAllocSize(ty) );
    }

    assert_leq( globalmem.size(), 0x300 ); // XXX 0x400 is the lowest valid
                                           // arena pointer... but that
                                           // shouldn't be hardcoded here of
                                           // all places
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
    locationIndex.insert( i++, Location( 0, 0, 0 ) );
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
