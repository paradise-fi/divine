// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/domains/sym.h>
#include <lart/abstract/intrinsic.h>
#include <lart/abstract/domains/domains.h>

namespace lart {
namespace abstract {

namespace {
    const std::string prefix = "__abstract_";
    std::string constructFunctionName( const Intrinsic & intr ) {
        std::string name = prefix + "sym_" + intr.name();
        if ( intr.name() == "load" ) {
            //do nothing
        }
        else if ( intr.declaration()->arg_size() > 0 && intr.argType< 0 >()->isPointerTy() ) {
            name += "_p";
        }
        return name;
    }

    llvm::ConstantInt * bitwidth( llvm::Type * type ) {
        auto & ctx = type->getContext();
        if ( isAbstract( type ) ) {
            auto sty = llvm::cast< llvm::StructType >( stripPtr( type ) );
            type = TypeName( sty ).base().llvm( ctx );
        }
        auto bw = llvm::cast< llvm::IntegerType >( type )->getBitWidth();
        return llvm::ConstantInt::get( llvm::IntegerType::get( ctx, 32 ), bw );
    }

    std::vector< llvm::Value * > arguments( llvm::CallInst * call,
                                            std::vector< llvm::Value * > & args )
    {
        auto intr = Intrinsic( call );
        const auto & n = intr.name();
        auto & ctx = call->getContext();

        if ( n == "alloca" )
        {
            auto type = typebase( intr.nameElement( 3 ) ).llvm( ctx );
            return { bitwidth( type ) };
        }
        else if ( n == "lift" )
        {
            llvm::IRBuilder<> irb( call );
            auto sext = irb.CreateSExt( args[ 0 ], llvm::IntegerType::get( ctx, 64 ) );
            return { sext, bitwidth( intr.argType< 0 >() ) };
        }
        else if ( n == "load" )
        {
            return { args[0],  bitwidth( intr.argType< 0 >() ) };
        }
        else if ( ( n == "store" ) ||
                  // binary operations
                  ( n == "add" ) ||
                  ( n == "sub" ) ||
                  ( n == "mul" ) ||
                  ( n == "sdiv" ) ||
                  ( n == "udiv" ) ||
                  ( n == "urem" ) ||
                  ( n == "srem" ) ||
                  ( n == "and" ) ||
                  ( n == "or" ) ||
                  ( n == "xor" ) ||
                  ( n == "shl" ) ||
                  ( n == "lshr" ) ||
                  ( n == "ashr" ) ||
                  // logic operations
                  ( n == "icmp_eq" ) ||
                  ( n == "icmp_ne" ) ||
                  ( n == "icmp_ugt" ) ||
                  ( n == "icmp_uge" ) ||
                  ( n == "icmp_ult" ) ||
                  ( n == "icmp_ule" ) ||
                  ( n == "icmp_sgt" ) ||
                  ( n == "icmp_sge" ) ||
                  ( n == "icmp_slt" ) ||
                  ( n == "icmp_sle" ) ||
                  // to_tristate
                  ( n == "bool_to_tristate" ) ||
                  // assume
                  ( n == "assume" ) )
        {
            return args;
        }
        else if ( ( n == "trunc" ) ||
                  ( n == "zext" ) ||
                  ( n == "sext" ) ||
                  ( n == "bitcast" ) )
        {
            auto type = typebase( intr.nameElement( 4 ) ).llvm( ctx );
            return { args[0], bitwidth( type ) };
        }
        else
        {
            std::cerr << "ERR: unknown instruction: ";
            call->dump();
            std::exit( EXIT_FAILURE );
        }
    }
}

Symbolic::Symbolic( llvm::Module & m ) {
    formula_type = m.getFunction( "__abstract_sym_load" )->getReturnType();
}

llvm::Value * Symbolic::process( llvm::CallInst * call, std::vector< llvm::Value * > & args ) {
    auto intr = Intrinsic( call );
    assert( (*intr.domain()) == (*domain()) );
    auto name = constructFunctionName( call );
    args = arguments( call, args );

    llvm::Module * m = intr.declaration()->getParent();

    llvm::Function * fn = m->getFunction( name );
    assert( fn && "Function for intrinsic substitution was not found." );
    assert( fn->arg_size() == args.size() );

    llvm::IRBuilder<> irb( call );
    return irb.CreateCall( fn, args );
}

bool Symbolic::is( llvm::Type * type ) {
    return formula_type == type;
}

llvm::Type * Symbolic::abstract( llvm::Type * type ) {
    return type->isPointerTy() ? formula_type->getPointerTo() : formula_type;
}

} // namespace abstract
} // namespace lart
