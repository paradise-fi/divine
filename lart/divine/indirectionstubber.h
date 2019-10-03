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
struct StubIndirection {
  llvm::Module &_module;
  llvm::LLVMContext &_context;

  uint64_t _counter = 0;
  const std::string wrap_prefix = "indirect.call.";

  struct StubLocation {
    llvm::Function *where;
    std::unordered_set< llvm::Function *> targets;
    llvm::BasicBlock *tail;
  };

  std::unordered_map< llvm::Function *, StubLocation > _func2tail;
  std::unordered_set< std::string > _wrappers;

  StubIndirection( llvm::Module &module ) :
    _module( module ), _context( module.getContext() ) {}

  // Get new terminal node
  llvm::BasicBlock *_CreateNewNode( llvm::Function *func ) {
    auto node = llvm::BasicBlock::Create( _context, "", func );

    llvm::IRBuilder<>{ node }.CreateUnreachable();
    return node;
  }

  llvm::Function *_Wrap( llvm::Type *original );

  void Stub();

  llvm::BasicBlock *_CreateCallBB( llvm::Function *where, llvm::Function *target );

  llvm::BasicBlock *_CreateCase( llvm::Function *where, llvm::Function *target );

  // Be sure that pointers are from the same Module!
  void _Enhance( llvm::Function *where, llvm::Function *target ) {
    ASSERT( _func2tail.count( where ),
           "Trying to enhance function that is not indirection.wrapper" );
    _func2tail[ where ].tail = _CreateCase( where, target );
  }

  void Enhance( llvm::Function *where, llvm::Function *target ) {
    // target is typically from clonned module -> we need to get declaration into our module
    auto target_func = llvm::dyn_cast< llvm::Function >(
        _module.getOrInsertFunction( target->getName(), target->getFunctionType() ) );

    _Enhance( _module.getFunction( where->getName() ), target_func );
  }

  void Enhance( std::pair< llvm::Function *, llvm::Function *> enhance ) {
    Enhance( enhance.first, enhance.second );
  }

  // We do matching on names, since we typically work with multiple modules
  bool IsWrapper( const std::string &name ) {
    return _wrappers.count( name );
  }

  bool IsWrapper( const llvm::Function *func ) {
    return func && IsWrapper( func->getName().str() );
  }
};

static inline std::ostream &operator<<( std::ostream &os, const StubIndirection &self ) {
  os << "List stubs: " << std::endl;
  for ( const auto &f : self._func2tail ) {
    os << "\t" << f.first->getName().str() << std::endl;
    f.first->dump();
  }
  return os;
}

} // namespace lart::divine
