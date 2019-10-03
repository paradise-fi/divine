#pragma once

#include <iostream>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <bricks/brick-llvm>

namespace lart::divine {

// TODO: With LLVM-8.0 we will need to add CallBr at multiple places
struct IndirectCallsStubber {

  llvm::Module &_module;
  llvm::LLVMContext &_ctx;

  uint64_t _counter = 0;
  const std::string wrap_prefix = "indirect.call.";

  // Information needed to properly build whole if-else tree in new dispatcher
  struct StubLocation {
    llvm::Function *where;
    std::unordered_set< llvm::Function *> targets;

    // Last block, i.e. default case of switch, contains only unreachable instruction
    llvm::BasicBlock *tail;
  };

  std::unordered_map< llvm::Function *, StubLocation > _func2tail;
  std::unordered_set< std::string > _wrappers;

  IndirectCallsStubber( llvm::Module &module ) :
    _module( module ), _ctx( module.getContext() ) {}

  // Get new terminal node
  llvm::BasicBlock *_createNewNode( llvm::Function *func ) {
    auto node = llvm::BasicBlock::Create( _ctx, "", func );

    llvm::IRBuilder<>{ node }.CreateUnreachable();
    return node;
  }

  void stub();

  llvm::Function *_wrap( llvm::Type *original );

  llvm::BasicBlock *_createCallBB( llvm::Function *where, llvm::Function *target );

  llvm::BasicBlock *_createCase( llvm::Function *where, llvm::Function *target );

  // Be sure that pointers are from the same Module!
  void _enhance( llvm::Function *where, llvm::Function *target ) {
    ASSERT( _func2tail.count( where ),
           "Trying to enhance function that is not indirection.wrapper" );
    _func2tail[ where ].tail = _createCase( where, target );
  }

  void enhance( llvm::Function *where, llvm::Function *target ) {
    // target is typically from clonned module -> we need to get declaration into our module
    auto target_func = llvm::dyn_cast< llvm::Function >(
        _module.getOrInsertFunction( target->getName(), target->getFunctionType() ) );

    _enhance( _module.getFunction( where->getName() ), target_func );
  }

  void enhance( std::pair< llvm::Function *, llvm::Function *> new_call ) {
    enhance( new_call.first, new_call.second );
  }

  // Matching on names, since we typically work with multiple modules
  bool isWrapper( const std::string &name ) {
    return _wrappers.count( name );
  }

  bool isWrapper( const llvm::Function *func ) {
    return func && isWrapper( func->getName().str() );
  }
};

static inline std::ostream &operator<<( std::ostream &os, const IndirectCallsStubber &self ) {
  os << "List stubs: " << std::endl;
  for ( const auto &f : self._func2tail ) {
    os << "\t" << f.first->getName().str() << std::endl;
    f.first->dump();
  }
  return os;
}

} // namespace lart::divine
