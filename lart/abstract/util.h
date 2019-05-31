// -*- C++ -*- (c) 2017 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/support/query.h>
#include <lart/support/util.h>
#include <lart/abstract/domain.h>

namespace lart::abstract
{
    #define ERROR( msg ) \
        throw std::runtime_error( msg );

    auto get_potentially_called_functions( llvm::CallInst* call ) -> std::vector< llvm::Function * >;
    auto get_some_called_function( llvm::CallInst * call ) -> llvm::Function *;

    template< const char * tag >
    auto calls_with_tag( llvm::Module & m )
    {
        return query::query( m ).flatten().flatten()
            .map( query::llvmdyncast< llvm::CallInst > )
            .filter( query::notnull )
            .filter( [&] ( auto * call ) {
                return call->getMetadata( tag );
            } )
            .freeze();
    }

    template< typename Fn >
    void run_on_potentially_called_functions( llvm::CallInst * call, Fn functor )
    {
        for ( auto fn : get_potentially_called_functions( call ) )
            functor( fn );
    }

    template< typename Fn >
    void run_on_abstract_calls( Fn functor, llvm::Module & m )
    {
        for ( auto * val : meta::enumerate( m ) ) {
            if ( auto call = llvm::dyn_cast< llvm::CallInst >( val ) ) {
                auto fns = get_potentially_called_functions( call );

                bool process = query::query( fns ).any( [] ( auto fn ) {
                    return fn != nullptr && !fn->isIntrinsic() && !fn->empty();
                } );

                if ( process )
                    functor( call );
            }
        }
    }

    using Types = std::vector< llvm::Type * >;
    using Values = std::vector< llvm::Value * >;
    using Functions = std::vector< llvm::Function * >;

    template< typename Values >
    Types types_of( const Values & vs )
    {
        return query::query( vs ).map( [] ( const auto & v ) {
            return v->getType();
        } ).freeze();
    }

    std::string llvm_name( llvm::Type * type );

    llvm::Type* abstract_type( llvm::Type * orig, Domain dom );

    namespace {
        using Predicate = llvm::CmpInst::Predicate;
    }

    static const std::unordered_map< llvm::CmpInst::Predicate, std::string > PredicateTable = {
        { Predicate::FCMP_FALSE, "false" },
        { Predicate::FCMP_OEQ, "oeq" },
        { Predicate::FCMP_OGT, "ogt" },
        { Predicate::FCMP_OGE, "oge" },
        { Predicate::FCMP_OLT, "olt" },
        { Predicate::FCMP_OLE, "ole" },
        { Predicate::FCMP_ONE, "one" },
        { Predicate::FCMP_ORD, "ord" },
        { Predicate::FCMP_UNO, "uno" },
        { Predicate::FCMP_UEQ, "ueq" },
        { Predicate::FCMP_UGT, "ugt" },
        { Predicate::FCMP_UGE, "uge" },
        { Predicate::FCMP_ULT, "ult" },
        { Predicate::FCMP_ULE, "ule" },
        { Predicate::FCMP_UNE, "une" },
        { Predicate::FCMP_TRUE, "true" },
        { Predicate::ICMP_EQ, "eq" },
        { Predicate::ICMP_NE, "ne" },
        { Predicate::ICMP_UGT, "ugt" },
        { Predicate::ICMP_UGE, "uge" },
        { Predicate::ICMP_ULT, "ult" },
        { Predicate::ICMP_ULE, "ule" },
        { Predicate::ICMP_SGT, "sgt" },
        { Predicate::ICMP_SGE, "sge" },
        { Predicate::ICMP_SLT, "slt" },
        { Predicate::ICMP_SLE, "sle" }
    };
} // namespace lart::abstract
