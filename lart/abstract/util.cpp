// -*- C++ -*- (c) 2018 Henrich Lauko <xlauko@mail.muni.cz>
#include <lart/abstract/util.h>

DIVINE_RELAX_WARNINGS
#include <llvm/IR/Type.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/lib/IR/LLVMContextImpl.h>
DIVINE_UNRELAX_WARNINGS

#include <unordered_map>
#include <brick-llvm>
#include <brick-string>

namespace lart::abstract
{
    std::string llvm_name( llvm::Type * type ) {
        std::string buffer;
        llvm::raw_string_ostream rso( buffer );
        type->print( rso );
        return rso.str();
    }

    // Tries to find precise set of possible called functions.
    // Returns true if it succeeded.
    bool resolve_function( llvm::Value *fn, Functions &fns )
    {
        if ( auto ce = llvm::dyn_cast< llvm::ConstantExpr >( fn ) )
            return resolve_function( ce->stripPointerCasts(), fns );

        if ( auto ga = llvm::dyn_cast< llvm::GlobalAlias >( fn ) )
            return resolve_function( ga->getBaseObject(), fns );

        if ( auto f = llvm::dyn_cast< llvm::Function >( fn ) )
            return fns.push_back( f ), true;

        if ( auto phi = llvm::dyn_cast< llvm::PHINode >( fn ) )
        {
            for ( auto & iv : phi->incoming_values() )
                if ( !resolve_function( iv.get(), fns ) )
                    return false;
            return true;
        }

        if ( llvm::isa< llvm::LoadInst >( fn ) || llvm::isa< llvm::Argument >( fn ) )
            return false;

        UNREACHABLE( "unknown parent instruction in function resolution", *fn );
    }

    Functions resolve_function( llvm::Module *m, llvm::Value *fn )
    {
        Functions fns;
        if ( resolve_function( fn, fns ) )
            return fns;

        auto type = fn->getType();

        // brute force all possible functions with correct signature
        return query::query( m->functions() )
            .filter( [type] ( auto & fn ) { return fn.getType() == type; } )
            .filter( [] ( auto & fn ) { return fn.hasAddressTaken(); } )
            .map( query::refToPtr )
            .freeze();
    }

    bool is_terminal_intruction( llvm::Value * val ) {
        return llvm::isa< llvm::ReturnInst >( val ) ||
               llvm::isa< llvm::UnreachableInst >( val );
    }

} // namespace lart::abstract
