// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/sym.h>
#include <lart/abstract/intrinsic.h>
#include <lart/abstract/domains/domains.h>
#include <lart/abstract/types.h>

namespace lart {
namespace abstract {

namespace {
    const std::string prefix = "__abstract_";
    std::string constructFunctionName( const llvm::CallInst * inst ) {
        std::string name = prefix + "sym_" + intrinsic::name( inst );
        if ( inst->getNumArgOperands() > 0 && intrinsic::ty1( inst ).back() == '*' )
            name += "_p";
        return name;
    }

    llvm::ConstantInt * bitwidth( llvm::LLVMContext & ctx, const std::string & type ) {
        int bw = std::stoi( type.substr(1) );
        return llvm::ConstantInt::get( llvm::IntegerType::get( ctx, 32 ), bw );
    }

    std::vector< llvm::Value * > arguments( llvm::CallInst * i,
                                            std::vector< llvm::Value * > & args )
    {
        const std::string & n = intrinsic::name( i );
        if ( n == "alloca" )
        {
            return { bitwidth( i->getContext(), intrinsic::ty1( i ) ) };
        }
        else if ( n == "lift" )
        {
            llvm::IRBuilder<> irb( i );
            auto sext = irb.CreateSExt( args[ 0 ], llvm::IntegerType::get(i->getContext(), 64) );
            return { sext, bitwidth( i->getContext(), intrinsic::ty1( i ) ) };
        }
        else if ( n == "load" )
        {
            return { args[0],  bitwidth( i->getContext(), intrinsic::ty1( i ) ) };
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
            return { args[0], bitwidth( i->getContext(), intrinsic::ty2( i ) ) };
        }
        else
        {
            std::cerr << "ERR: unknown instruction: ";
            i->dump();
            std::exit( EXIT_FAILURE );
        }
    }
}

Symbolic::Symbolic( llvm::Module & m ) {
    formula_type = m.getFunction( "__abstract_sym_load" )->getReturnType();
}

llvm::Value * Symbolic::process( llvm::CallInst * i, std::vector< llvm::Value * > & args ) {
    std::string name;
    if ( intrinsic::domain( i ) == Domain::Value::Tristate ) {
        name = "__abstract_tristate_lower";
    } else {
        name = constructFunctionName( i );
        args = arguments( i, args );
    }

    llvm::Module * m = i->getParent()->getParent()->getParent();

    llvm::Function * fn = m->getFunction( name );
    assert( fn && "Function for intrinsic substitution was not found." );
    assert( fn->arg_size() == args.size() );

    llvm::IRBuilder<> irb( i );
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
