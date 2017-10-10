// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/domains/sym.h>
#include <lart/abstract/intrinsic.h>
#include <lart/abstract/util.h>
#include <lart/abstract/domains/domains.h>

#include <iostream>

namespace lart {
namespace abstract {

namespace {
    static const std::string prefix = "__abstract_";

    template< size_t idx >
    inline llvm::Type * argType( llvm::Instruction * i ) {
        return i->getOperand( idx )->getType();
    }

    std::string constructFunctionName( llvm::CallInst * call ) {
        auto intr = intrinsic::parse( call->getCalledFunction() ).value();
        std::string name = prefix + "sym_" + intr.name;
        if ( intr.name == "load" ) {
            //do nothing
        } else if ( call->getCalledFunction()->arg_size() > 0 && argType< 0 >( call )->isPointerTy() ) {
            name += "_p";
        }
        return name;
    }
}

llvm::ConstantInt * Symbolic::bitwidth( llvm::Type * type ) {
    auto o = tmap.isAbstract( type ) ? origin( type ) : type;
    auto& ctx = type->getContext();
    auto bw = llvm::cast< llvm::IntegerType >( o )->getBitWidth();
    return llvm::ConstantInt::get( llvm::IntegerType::get( ctx, 32 ), bw );
}

Values Symbolic::arguments( llvm::CallInst * call , Args & args ) {
    auto intr = intrinsic::parse( call->getCalledFunction() ).value();
    assert( intr.domain == domain() );
    const auto & n = intr.name;
    auto & ctx = call->getContext();

    if ( n == "alloca" ) {
        return { bitwidth( stripPtr( call->getType() ) ) };
    } else if ( n == "lift" ) {
        llvm::IRBuilder<> irb( call );
        auto sext = irb.CreateSExt( args[ 0 ], llvm::IntegerType::get( ctx, 64 ) );
        return { sext, bitwidth( stripPtr( argType< 0 >( call ) ) ) };
    } else if ( n == "load" ) {
        return { args[0], bitwidth( stripPtr( argType< 0 >( call ) ) ) };
    } else if ( ( n == "store" ) ||
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
    } else if ( ( n == "trunc" ) ||
              ( n == "zext" ) ||
              ( n == "sext" ) ||
              ( n == "bitcast" ) )
    {
        return { args[0], bitwidth( stripPtr( call->getType() ) ) };
    } else {
        std::cerr << "ERR: unknown instruction: ";
        call->dump();
        std::exit( EXIT_FAILURE );
    }
}

llvm::Value * Symbolic::process( llvm::CallInst * call, Values & args ) {
    auto name = constructFunctionName( call );
    args = arguments( call, args );

    auto m = getModule( call );

    llvm::Function * fn = m->getFunction( name );
    assert( fn && "Function for intrinsic substitution was not found." );

    llvm::IRBuilder<> irb( call );
    return irb.CreateCall( fn, args );
}

bool Symbolic::is( llvm::Type * type ) {
    return formula_type == type;
}

llvm::Type * Symbolic::abstract( llvm::Type * type ) {
    return setPointerRank( formula_type, getPointerRank( type ) );
}

} // namespace abstract
} // namespace lart
