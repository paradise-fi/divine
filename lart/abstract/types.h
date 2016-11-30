// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/lib/IR/LLVMContextImpl.h>
#include <llvm/IR/TypeBuilder.h>

#include <llvm/Support/raw_ostream.h>
DIVINE_UNRELAX_WARNINGS

#include <string>
#include <sstream>

#ifndef LART_ABSTRACTION_TYPES_H
#define LART_ABSTRACTION_TYPES_H

namespace lart {
namespace abstract {

namespace {

std::string str( llvm::Type * type ) {
    std::string type_str;
    llvm::raw_string_ostream rso( type_str );
    type->print( rso );
    return rso.str();
}

}

struct Base {
    static const std::string name() { return "lart"; }
};

struct IntegerType : Base {
    static std::string name( llvm::Type * type ) {
        assert( type->isIntegerTy() );
        return IntegerType::name( llvm::cast< llvm::IntegerType >( type )->getBitWidth() );
    }

    static std::string name( unsigned bw ) {
        return Base::name() + ".abstract." + std::to_string( bw );
    }

    static llvm::StructType * get( llvm::Type * type ) {
        assert( type->isIntegerTy() );
        auto ity = llvm::cast< llvm::IntegerType >( type );
        return IntegerType::get(  ity->getContext(), ity->getBitWidth() );
    }

    static llvm::StructType * get( llvm::LLVMContext & ctx, unsigned bw ) {
        if( auto lookup = ctx.pImpl->NamedStructTypes.lookup( name( bw ) ) )
            return lookup;
        return llvm::StructType::create( ctx, IntegerType::name( bw ) );
    }

    static unsigned bw( llvm::StructType * type ) {
        assert( type->getElementType( 0 )->isIntegerTy() );
        return llvm::cast< llvm::IntegerType >( type->getElementType( 0 ) )->getBitWidth();
    }

    static bool isa( llvm::Type * type ) {
        if ( ! type->isStructTy() )
            return false;
        auto sty = llvm::cast< llvm::StructType >( type );
        if ( ! sty->getElementType( 0 )->isIntegerTy() )
            return false;
        return IntegerType::get( type->getContext(), IntegerType::bw( sty ) ) == type;
    }
};

struct Tristate : Base {
    static std::string name() {
        return Base::name() + ".tristate";
    }

    static llvm::StructType * get( llvm::LLVMContext & ctx ) {
        if( auto lookup = ctx.pImpl->NamedStructTypes.lookup( name() ) )
            return lookup;
        return llvm::StructType::create( { body( ctx ) }, Tristate::name() );
    }

    static bool isa( llvm::Type * type ) {
        return Tristate::get( type->getContext() ) == type;
    }

    static llvm::Constant * True() {
        auto & ctx = llvm::getGlobalContext();
        return llvm::ConstantStruct::get( get( ctx ), { llvm::ConstantInt::getTrue( ctx ) } );
    }

    static llvm::Constant * False() {
        auto & ctx = llvm::getGlobalContext();
        return llvm::ConstantStruct::get( get( ctx ), { llvm::ConstantInt::getFalse( ctx ) } );
    }

private:
    static llvm::IntegerType * body( llvm::LLVMContext & ctx ) {
        return llvm::IntegerType::get( ctx, 1 );
    }

};

static std::string getTypeName( llvm::Type * type ) {
    std::string buffer;
    llvm::raw_string_ostream rso( buffer );
    type->print( rso );
    return rso.str();
}

static std::vector< std::string > parseTypeName( llvm::Type * type ) {
    std::stringstream ss;
    auto structT = llvm::cast< llvm::StructType >( type );
    ss.str( structT->getName().str() );
    std::vector< std::string > parts;
    std::string part;
    while( std::getline( ss, part, '.' ) )
        parts.push_back( part );
    return parts;
}

static std::string typeQualifier( llvm::Type * type ) {
    auto parts = parseTypeName( type );
    assert( parts.size() >= 3 );
    return parts[ 2 ];
}

} /* lart */
} /* abstract */

#endif // LART_ABSTRACTION_TYPES_H
