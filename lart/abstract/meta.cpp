#include <lart/abstract/meta.h>

namespace lart::abstract::meta {

namespace {
    void set_value_as_meta( llvm::Instruction * inst, const std::string& tag, llvm::Value * val ) {
        auto &ctx = inst->getContext();
        auto meta = meta::tuple::create( ctx, { llvm::ValueAsMetadata::get( val ) } );
        inst->setMetadata( tag, meta );
    }

    llvm::Value * get_value_from_meta( llvm::Instruction * inst, const std::string& tag ) {
        auto &meta = inst->getMetadata( tag )->getOperand( 0 );
        return llvm::cast< llvm::ValueAsMetadata >( meta.get() )->getValue();
    }
} // anonymous namespace

    }

    }


    namespace function {
        void init( llvm::Function * fn ) noexcept {
            auto size = fn->arg_size();

            auto & ctx = fn->getContext();

            if ( !fn->getMetadata( meta::tag::function ) ) {
                auto value = [&] { return meta::value::create( ctx, "" ); };
                auto data = meta::tuple::create( ctx, size, value );
                fn->setMetadata( meta::tag::function, data );
            }
        }

        void clear( llvm::Function * fn ) noexcept {
            if ( fn->getMetadata( meta::tag::function ) )
                fn->setMetadata( meta::tag::function, nullptr );
        }

        bool ignore_call( llvm::Function * fn ) noexcept {
            return fn->getMetadata( tag::transform::ignore::arg );
        }

        bool ignore_return( llvm::Function * fn ) noexcept {
            return fn->getMetadata( tag::transform::ignore::ret );
        }

        bool is_forbidden( llvm::Function * fn ) noexcept {
            return fn->getMetadata( tag::transform::forbidden );
        }
    } // namespace function

    namespace argument {
        void set( llvm::Argument * arg, const std::string & str ) noexcept {
            auto & ctx = arg->getContext();
            argument::set( arg, meta::value::create( ctx, str ) );
        }

        void set( llvm::Argument * arg, llvm::MDNode * node ) noexcept {
            auto fn = arg->getParent();
            function::init( fn );

            auto meta = fn->getMetadata( meta::tag::function );
            meta->replaceOperandWith( arg->getArgNo(), node );
        }

        auto get( llvm::Argument * arg ) noexcept -> std::optional<std::string> {
            auto fn = arg->getParent();
            if ( auto node = fn->getMetadata( meta::tag::function ) ) {
                return meta::value::get( node, arg->getArgNo() );
            }

            return std::nullopt;
        }

        bool has( llvm::Argument * arg ) noexcept {
            auto fn = arg->getParent();
            if ( auto node = fn->getMetadata( meta::tag::function ) ) {
                return !meta::value::get( node, arg->getArgNo() ).empty();
            }

            return false;
        }
    } // namespace argument

    void make_duals( llvm::Instruction * a, llvm::Instruction * b ) {
        set_value_as_meta( a, meta::tag::dual, b );
        set_value_as_meta( b, meta::tag::dual, a );
    }

    llvm::Value * get_dual( llvm::Instruction *inst ) {
        return get_value_from_meta( inst, meta::tag::dual );
    }

} // namespace lart::abstract::meta
