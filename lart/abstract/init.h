// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <lart/abstract/util.h>
#include <optional>


namespace lart::abstract {

    constexpr const char * domain_index = "lart.abstract.domain.index";

    struct InitAbstractions {

        using Namespace = llvm::StringRef;

        struct Domain;

        struct Operation
        {
            llvm::Function * impl;
            Domain * domain;

            Operation( llvm::Function * impl, Domain * dom )
                : impl( impl ), domain( dom )
            {}

            llvm::StringRef name() const;

            inline bool is_op() const { return name().startswith( "op_" ); }
            inline bool is_fn() const { return name().startswith( "fn_" ); }
        };

        struct Domain
        {
            using index_t = uint8_t;

            Namespace ns;
            std::vector< Operation > operations;

            Domain( Namespace ns )
                : ns( ns ), operations( {} )
            {}

            std::optional< Operation > get_operation( llvm::StringRef name ) const;
            void annotate( index_t index ) const;
            Operation lift() const;

            index_t index() const;
            llvm::ConstantInt * llvm_index() const;
        };

        using Domains = std::vector< std::unique_ptr< Domain > >;

        void run( llvm::Module &m );

        void annotate( const Domains & domains ) const;

        Domains domains() const;

        llvm::Module * _m;
    };

} // namespace lart::abstract
