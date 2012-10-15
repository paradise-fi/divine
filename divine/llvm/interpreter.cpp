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

#include <divine/llvm/interpreter.h>
#include "llvm/CodeGen/IntrinsicLowering.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Constants.h"
#include "llvm/Module.h"
#include <cstring>
using namespace llvm;
using namespace divine::llvm;

//===----------------------------------------------------------------------===//
// Interpreter ctor - Initialize stuff
//
Interpreter::Interpreter(Module *M)
    : ExecutionEngine(M), TD(M), module( M )
{
  memset(&ExitValue.Untyped, 0, sizeof(ExitValue.Untyped));
  setTargetData(&TD);

  initializeExecutionEngine();

  emitGlobals( M );
  buildIndex( M );

  threads.resize( 1 );
  thread( 0 ).id = 1;
  _context = 0;
  _alternative = 0;
  flags.assert = flags.invalid_argument = false;
  flags.invalid_dereference = flags.null_dereference = false;

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

        // GlobalVariable type is always a pointer, so dereference it first
        Type *ty = var->getType()->getTypeAtIndex(unsigned(0));
        assert( ty );

        assert( !var->isDeclaration() ); // can't handle extern's yet
        if ( std::string( var->getNameStr(), 0, 6 ) == "__LTL_" ) {
            std::string name( var->getNameStr(), 6, std::string::npos );
            GenericValue GV = getConstantValue(var->getInitializer());
            properties[name] = std::string( (char*) GV.PointerVal );
            continue; // do not emit this one
        }

        globals.insert( std::make_pair( &*var, globalmem.size() + 0x100 ) );
        globalmem.resize( globalmem.size() + TD.getTypeAllocSize(ty) );
    }

    assert_leq( globalmem.size(), 0x300 ); // XXX 0x400 is the lowest valid
                                           // arena pointer... but that
                                           // shouldn't be hardcoded here of
                                           // all places
}

void* Interpreter::getOrEmitGlobalVariable(const GlobalVariable *GV) {
    if (Function *F = const_cast<Function*>(dyn_cast<Function>(GV)))
        return getPointerToFunction(F);

    std::map< const GlobalVariable *, int >::const_iterator i = globals.find( GV );
    if  (i == globals.end()) {
        // not stored in globalmem => constant global stored internally in ExecutionEngine
        return getPointerToGlobal(GV);
    } else {
        return (void*) i->second;
    }
}

void Interpreter::buildIndex( Module *module ) {
    int i = 0;
    locationIndex.insert( i++, Location( 0, 0, 0 ) );
    for ( Module::iterator function = module->begin(); function != module->end(); ++function ) {
        functionIndex.insert( i++, &*function );
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
