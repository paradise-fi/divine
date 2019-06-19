// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/syntactic.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/util.h>
#include <lart/abstract/util.h>
#include <lart/abstract/operation.h>

namespace lart::abstract
{

    bool is_faultable( llvm::Instruction * inst )
    {
        using Inst = llvm::Instruction;
        if ( auto bin = llvm::dyn_cast< llvm::BinaryOperator >( inst ) ) {
            auto op = bin->getOpcode();
            return op == Inst::UDiv ||
                   op == Inst::SDiv ||
                   op == Inst::FDiv ||
                   op == Inst::URem ||
                   op == Inst::SRem ||
                   op == Inst::FRem;
        }

        return llvm::isa< llvm::CallInst >( inst );
    }

    void Syntactic::run( llvm::Module &m ) {
        auto abstract = query::query( meta::enumerate( m ) )
            .map( query::llvmdyncast< llvm::Instruction > )
            .filter( query::notnull )
            .freeze();

        OperationBuilder builder;
        for ( auto * inst : abstract ) {
            if ( auto call = llvm::dyn_cast< llvm::CallInst >( inst ) ) {
                if ( call->getCalledFunction()->getMetadata( meta::tag::abstract ) )
                    continue;
            }

            assert( !is_faultable( inst ) );
            // TODO replace faultable operations by lifter handler
            // TODO annotate it as abstract return function to unstash its value

            builder.construct( inst );
        }
    }

} // namespace lart::abstract
