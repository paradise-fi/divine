// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <optional>

namespace lart::abstract::meta {
    namespace tag {
        constexpr char abstract[] = "lart.abstract";
        constexpr char roots[] = "lart.abstract.roots";

        constexpr char function[] = "lart.abstract.function";

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

    using MetaVal = std::optional< std::string >;

    /* creates metadata node from given string */
    llvm::MDNode * create( llvm::LLVMContext & ctx, const std::string & str ) noexcept;

    /* Gets stored metadata string from position 'idx' from metadata node */
    MetaVal value( llvm::MDNode * node, unsigned idx = 0 ) noexcept;

    /* Metadata are stored as map, where tag points to metadata tuple */

    /* checks whether values has metadata for given tag */
    bool has( llvm::Value * val, const std::string & tag ) noexcept;
    bool has( llvm::Argument * arg, const std::string & tag ) noexcept;
    bool has( llvm::Instruction * inst, const std::string & tag ) noexcept;

    /* gets metadata string from value for given meta tag */
    MetaVal get( llvm::Value * val, const std::string & tag ) noexcept;
    MetaVal get( llvm::Argument * arg, const std::string & tag ) noexcept;
    MetaVal get( llvm::Instruction * inst, const std::string & tag ) noexcept;

    /* sets metadata with tag to value */
    void set( llvm::Value * val, const std::string & tag, const std::string & meta ) noexcept;
    void set( llvm::Argument * arg, const std::string & tag, const std::string & meta ) noexcept;
    void set( llvm::Instruction * inst, const std::string & tag, const std::string & meta ) noexcept;

    namespace abstract
    {
        /* works with metadata with tag meta::tag::domains */

        bool has( llvm::Value * val ) noexcept;
        bool has( llvm::Argument * arg ) noexcept;
        bool has( llvm::Instruction * inst ) noexcept;

        MetaVal get( llvm::Value * val ) noexcept;
        MetaVal get( llvm::Argument * arg ) noexcept;
        MetaVal get( llvm::Instruction * inst ) noexcept;

        void set( llvm::Value * val, const std::string & meta ) noexcept;
        void set( llvm::Argument * arg, const std::string & meta ) noexcept;
        void set( llvm::Instruction * inst, const std::string & meta ) noexcept;
    }

    namespace tuple
    {
        static llvm::MDTuple * create( llvm::LLVMContext & ctx,
                                       const llvm::ArrayRef< llvm::Metadata * > vals )
        {
            assert( !vals.empty() );
            return llvm::MDTuple::get( ctx, vals );
        }

        template< typename Init >
        static llvm::MDTuple * create( llvm::LLVMContext & ctx, unsigned size, Init init ) {
            std::vector< llvm::Metadata* > values( size );
            std::generate( values.begin(), values.end(), init );
            return llvm::MDTuple::get( ctx, values );
        }

        static llvm::MDTuple * empty( llvm::LLVMContext &ctx ) {
            return llvm::MDTuple::get( ctx, {} );
        }
    };


    namespace function
    {
        bool ignore_call( llvm::Function * fn ) noexcept;
        bool ignore_return( llvm::Function * fn ) noexcept;
        bool is_forbidden( llvm::Function * fn ) noexcept;
    } // namespace function


    namespace argument
    {
        void set( llvm::Argument * arg, const std::string & str ) noexcept;
        void set( llvm::Argument * arg, llvm::MDNode * node ) noexcept;

        MetaVal get( llvm::Argument * arg ) noexcept;

        bool has( llvm::Argument * arg ) noexcept;
    } // namespace argument

    void make_duals( llvm::Instruction * a, llvm::Instruction * b );
    llvm::Value * get_dual( llvm::Instruction *inst );

} // namespace lart::abstract::meta
