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

    struct InitAbstractions : LLVMUtil< InitAbstractions > {

        using Namespace = llvm::StringRef;

        using Util = LLVMUtil< InitAbstractions >;

        using Util::i1Ty;
        using Util::i8Ty;
        using Util::i16Ty;
        using Util::i32Ty;
        using Util::i64Ty;

        using Util::i8PTy;

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
            inline bool is_lift() const { return name().startswith( "lift" ); }
            inline bool is_to_tristate() const { return name() == "to_tristate"; }
            inline bool is_assume() const { return name() == "assume"; }

            inline bool is() const {
                return is_op() || is_fn() || is_lift() || is_to_tristate() || is_assume();
            }
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

        void generate_get_domain_operation_intrinsic() const;

        llvm::Module * module;
    };

} // namespace lart::abstract
