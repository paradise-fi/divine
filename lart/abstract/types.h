// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@fi.muni.cz>
DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/TypeBuilder.h>
DIVINE_UNRELAX_WARNINGS

#include <string>

#ifndef LART_ABSTRACTION_TYPES_H
#define LART_ABSTRACTION_TYPES_H

namespace lart {
namespace abstract {

using T = llvm::Type;

auto str( T *type ) -> std::string {
    std::string type_str;
    llvm::raw_string_ostream rso( type_str );
    type->print( rso );
    return rso.str();
}

struct Base {
    static const std::string name() { return "lart"; }
};

struct Type : Base {
    static std::string name( T *type ) {
        return Base::name() + ".abstract." + str( type );
    }

    static llvm::StructType * get( T *type ) {
        return llvm::StructType::create( llvm::IntegerType::get( type->getContext(), 8 ),
                                         Type::name( type ) );
    }
};

struct Tristate : Base {
    static std::string name() {
        return Base::name() + ".tristate";
    }

    static llvm::StructType * get( T *type ) {
        return llvm::StructType::create( llvm::IntegerType::get( type->getContext(), 8 ),
                                         Tristate::name() );
    }

};

std::string getTypeName( llvm::Type * type ) {
    std::string buffer;
    llvm::raw_string_ostream rso( buffer );
    type->print( rso );
    return rso.str();
}

} /* lart */
} /* abstract */

#endif // LART_ABSTRACTION_TYPES_H
