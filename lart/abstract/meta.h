// -*- C++ -*- (c) 2019 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
DIVINE_UNRELAX_WARNINGS

#include <brick-except>

#include <optional>
#include <lart/support/query.h>

namespace lart::abstract::meta {
    namespace tag {
        constexpr char abstract[] = "lart.abstract"; // TODO change to return
        constexpr char roots[] = "lart.abstract.roots";

        namespace function {
            constexpr char arguments[] = "lart.abstract.function.arguments";
        }

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
            constexpr char spec[] = "lart.abstract.domain.spec";
            constexpr char name[] = "lart.abstract.domain.name";
            constexpr char kind[] = "lart.abstract.domain.kind";
        }

        namespace operation {
            constexpr const char function[] = "lart.op.function";
            constexpr const char type[] = "lart.op.type";
            constexpr const char index[] = "lart.op.index";

            constexpr const char freeze[] = "lart.op.freeze";
            constexpr const char thaw[] = "lart.op.thaw";

            constexpr const char phi[] = "lart.op.phi";
        }

        constexpr char abstract_return[] = "lart.abstract.return";
        constexpr char abstract_arguments[] = "lart.abstract.arguments";
    } // namespace tag

    using MetaVal = std::optional< std::string >;

    /* assigns metadata from annotations */
    void create_from_annotation( llvm::StringRef anno, llvm::Module &m ) noexcept;

    /* creates metadata node from given string */
    llvm::MDNode * create( llvm::LLVMContext & ctx, const std::string & str ) noexcept;

    /* Gets stored metadata string from position 'idx' from metadata node */
    MetaVal value( llvm::MDNode * node, unsigned idx = 0 ) noexcept;

    /* Metadata are stored as map, where tag points to metadata tuple */

    /* gets metadata string from value for given meta tag */
    MetaVal get( llvm::Value * val, const std::string & tag ) noexcept;
    MetaVal get( llvm::Argument * arg, const std::string & tag ) noexcept;
    MetaVal get( llvm::Instruction * inst, const std::string & tag ) noexcept;
    MetaVal get( llvm::Function * fn, const std::string & tag ) noexcept;
    MetaVal get( llvm::GlobalVariable * glob, const std::string & tag ) noexcept;

    llvm::Value * get_value_from_meta( llvm::Instruction * inst, const std::string & tag );

    /* checks whether values has metadata for given tag */
    bool has( llvm::Value * val, const std::string & tag ) noexcept;

    /* sets metadata with tag to value */
    void set( llvm::Value * val, const std::string & tag ) noexcept;
    void set( llvm::Value * val, const std::string & tag, const std::string & meta ) noexcept;
    void set( llvm::Argument * arg, const std::string & tag, const std::string & meta ) noexcept;
    void set( llvm::Instruction * inst, const std::string & tag, const std::string & meta ) noexcept;
    void set( llvm::Function * fn, const std::string & tag, const std::string & meta ) noexcept;

    void set_value_as_meta( llvm::Instruction * inst, const std::string & tag, llvm::Value * val );

    namespace tuple
    {
        static llvm::MDTuple * create( llvm::LLVMContext & ctx,
                                       const llvm::ArrayRef< llvm::Metadata * > vals )
        {
            assert( !vals.empty() );
            return llvm::MDTuple::getDistinct( ctx, vals );
        }

        template< typename Init >
        static llvm::MDTuple * create( llvm::LLVMContext & ctx, unsigned size, Init init ) {
            std::vector< llvm::Metadata* > values( size );
            std::generate( values.begin(), values.end(), init );
            return llvm::MDTuple::getDistinct( ctx, values );
        }

        static llvm::MDTuple * empty( llvm::LLVMContext &ctx ) {
            return llvm::MDTuple::getDistinct( ctx, {} );
        }
    } // namespace tuple

    namespace abstract
    {
        constexpr auto * tag = meta::tag::abstract;

        /* checks whether function has meta::tag::roots */
        bool roots( llvm::Function * fn );

        /* works with metadata with tag meta::tag::domains */

        template< typename T >
        bool has( T * val ) noexcept { return meta::has( val, abstract::tag ); }

        template< typename T >
        MetaVal get( T * val ) noexcept { return meta::get( val, abstract::tag ); }

        template< typename T >
        void set( T * value, const std::string & meta ) noexcept {
            if ( auto inst = llvm::dyn_cast< llvm::Instruction >( value ) ) {
                auto& ctx = inst->getContext();
                auto data = meta::tuple::empty( ctx );
                inst->getFunction()->setMetadata( meta::tag::roots, data );
            }
            meta::set( value, abstract::tag, meta );
        }

        void inherit( llvm::Value * dest, llvm::Value * src ) noexcept;
    } // namespace abstract

    namespace function
    {
        bool ignore_call( llvm::Function * fn ) noexcept;
        bool ignore_return( llvm::Function * fn ) noexcept;
        bool is_forbidden( llvm::Function * fn ) noexcept;
        bool operation( llvm::Function * fn ) noexcept;
    } // namespace function

    namespace argument
    {
        void set( llvm::Argument * arg, const std::string & str ) noexcept;
        void set( llvm::Argument * arg, llvm::Metadata * node ) noexcept;

        MetaVal get( llvm::Argument * arg ) noexcept;

        bool has( llvm::Argument * arg ) noexcept;
    } // namespace argument

    template< typename T >
    auto enumerate( T & llvm, std::string tag = meta::tag::abstract ) noexcept
        -> std::vector< llvm::Instruction * >
    {
        if constexpr ( std::is_same_v< T, llvm::Module > ) {
            auto enumerate = [=] ( auto & data ) { return meta::enumerate( data, tag ); };
            return query::query( llvm ).map( enumerate ).flatten().freeze();
        } else {
            static_assert( std::is_same_v< T, llvm::Function > );
            return query::query( llvm ).flatten()
                .map( query::refToPtr )
                .filter( [&] ( auto val ) {
                    return meta::has( val, tag );
                } )
                .freeze();
        }
    }

    template< typename I, typename T >
    auto enumerate( T & llvm, const std::string & tag )
    {
        return query::query( enumerate( llvm, tag ) )
            .map( query::llvmdyncast< I > )
            .freeze();
    }

    template< typename Tag >
    inline llvm::ConstantInt * llvm_index( llvm::Function * fn, const Tag & tag )
    {
        using namespace lart::abstract;

        auto i = fn->getMetadata( tag );
        if ( !i )
            brq::raise() << "Missing index metadata. " << tag;
        auto c = llvm::cast< llvm::ConstantAsMetadata >( i->getOperand( 0 ) );
        return llvm::cast< llvm::ConstantInt >( c->getValue() );
    }

    inline llvm::ConstantInt * operation_index( llvm::Function * fn )
    {
        using namespace lart::abstract;
        return llvm_index( fn, meta::tag::operation::index );
    }

} // namespace lart::abstract::meta
