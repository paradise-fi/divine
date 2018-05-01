// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <unordered_set>

#include <lart/abstract/util.h>

namespace lart {
namespace abstract {

struct Stash {
    void run( llvm::Module& );

private:
    void ret_stash( llvm::CallInst* );
    void ret_unstash( llvm::CallInst* );
    void arg_unstash( llvm::CallInst* );
    void arg_stash( llvm::CallInst* );

    std::unordered_set< llvm::Function* > stashed;
};

inline llvm::Function* unstash_function( llvm::Module *m ) {
    auto &ctx = m->getContext();
    auto rty = llvm::Type::getInt64Ty( ctx );
    auto fty = llvm::FunctionType::get( rty, {}, false );
    return get_or_insert_function( m, fty, "__lart_unstash" );
}

inline llvm::Function* stash_function( llvm::Module *m ) {
    auto &ctx = m->getContext();
    auto ty = llvm::Type::getInt64Ty( ctx );
    auto rty = llvm::Type::getVoidTy( ctx );
    auto fty = llvm::FunctionType::get( rty, { ty }, false );
    return get_or_insert_function( m, fty, "__lart_stash" );
}


} // namespace abstract
} // namespace lart
