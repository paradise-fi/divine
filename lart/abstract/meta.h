// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

namespace lart::abstract::meta {
    namespace tag {
        constexpr char abstract[] = "lart.abstract";
        constexpr char roots[] = "lart.abstract.roots";

        namespace transform {
            constexpr char prefix[] = "lart.transform";

            namespace ignore {
                constexpr char ret[] = "lart.transform.ignore.ret";
                constexpr char arg[] = "lart.transform.ignore.arg";
            } // namespace ignore

            constexpr char forbidden[] = "lart.transform.forbidden";
        } // namespace transform

        constexpr char domains[]  = "lart.abstract.domains";

        namespace domain {
            constexpr char name[] = "lart.abstract.domain.name";
            constexpr char kind[] = "lart.abstract.domain.kind";
        }

        namespace placeholder {
            constexpr const char type[] = "lart.placeholder.type";
            constexpr const char level[] = "lart.placeholder.level";
        }

        constexpr char dual[] = "lart.dual";
    } // namespace tag


    struct tuple
    {
        static llvm::MDTuple * create( llvm::LLVMContext & ctx,
                                       const llvm::ArrayRef< llvm::Metadata * > vals )
        {
            assert( !vals.empty() );
            return llvm::MDTuple::get( ctx, vals );
        }

        template< typename Init >
        static llvm::MDTuple * create( llvm::LLVMContext &ctx, unsigned size, Init init ) {
            std::vector< llvm::Metadata* > values( size );
            std::generate( values.begin(), values.end(), init );
            return llvm::MDTuple::get( ctx, values );
        }

        static llvm::MDTuple * empty( llvm::LLVMContext &ctx ) {
            return llvm::MDTuple::get( ctx, {} );
        }
    };


    struct value
    {
        static std::string get( llvm::MDNode * node ) {
            auto & op = node->getOperand( 0 );
            auto & str = llvm::cast< llvm::MDNode >( op )->getOperand( 0 );
            return llvm::cast< llvm::MDString >( str )->getString().str();
        }

        static llvm::MDNode * create( llvm::LLVMContext &ctx, const std::string & str ) {
            return llvm::MDNode::get( ctx, llvm::MDString::get( ctx, str ) );
        }
    };


    void set_llvm_value( llvm::Instruction * inst, const std::string& tag, llvm::Value * val );
    llvm::Value * get_llvm_value( llvm::Instruction * inst, const std::string& tag );

    bool ignore_call_of_function( llvm::Function * fn );
    bool ignore_return_of_function( llvm::Function * fn );
    bool is_forbidden_function( llvm::Function * fn );

    void make_duals( llvm::Instruction * a, llvm::Instruction * b );
    llvm::Value * get_dual( llvm::Instruction *inst );

} // namespace lart::abstract::meta
