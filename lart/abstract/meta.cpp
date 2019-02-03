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

    bool ignore_call_of_function( llvm::Function * fn ) {
        return fn->getMetadata( tag::transform::ignore::arg );
    }

    bool ignore_return_of_function( llvm::Function * fn ) {
        return fn->getMetadata( tag::transform::ignore::ret );
    }

    bool is_forbidden_function( llvm::Function * fn ) {
        return fn->getMetadata( tag::transform::forbidden );
    }

    void make_duals( llvm::Instruction * a, llvm::Instruction * b ) {
        set_value_as_meta( a, meta::tag::dual, b );
        set_value_as_meta( b, meta::tag::dual, a );
    }

    llvm::Value * get_dual( llvm::Instruction *inst ) {
        return get_value_from_meta( inst, meta::tag::dual );
    }

} // namespace lart::abstract::meta
