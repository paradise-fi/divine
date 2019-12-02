// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>
#include <lart/abstract/dfa.h>

namespace lart::abstract {

    /* Lift undef values of abstract insertvalues */
    struct UndefLifting
    {
        void run( llvm::Module & m );
        void process( llvm::InsertValueInst * insert );

        struct lift_function
        {
            meta::MetaVal meta;
            llvm::Function * function;

            static inline lift_function get( std::string name, llvm::Module & m )
            {
                lift_function lift;
                lift.function = m.getFunction( name );
                lift.meta = meta::get( lift.function, meta::tag::abstract );
                return lift;
            }

            llvm::CallInst * call( llvm::IRBuilder<> & irb )
            {
                auto lifted = irb.CreateCall( function );
                meta::set( lifted, meta::tag::abstract, meta.value() );
                return lifted;
            }
        };

        type_map _types;
        lift_function _lift;
    };

} // namespace lart::abstract
