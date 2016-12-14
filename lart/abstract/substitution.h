// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/trivial.h>
#include <lart/support/pass.h>
#include <lart/support/meta.h>

DIVINE_RELAX_WARNINGS
#include <llvm/Pass.h>
DIVINE_UNRELAX_WARNINGS

namespace lart {
namespace abstract {

struct Substitution : lart::Pass
{
    enum Type { Trivial };

    Substitution( Type t ) : _type( t ) {
        /*switch ( _type ) {
            case Trivial: {
                builder = SubstitutionBuilder(
                          std::unique_ptr< Common >( new abstract::Trivial() ) );
                break;
            }
        }*/
    }

    virtual ~Substitution() {}

    static PassMeta meta() {
    	return passMetaC( "Substitution",
                "Substitutes abstract values by concrete abstraction.",
                []( llvm::ModulePassManager &mgr, std::string opt ) {
                    return mgr.addPass( Substitution( getType( opt ) ) );
                } );
    }

    static Type getType( std::string ) {
        return Type::Trivial;
    }

    llvm::PreservedAnalyses run( llvm::Module & ) override;

private:
    llvm::Value * process( llvm::Argument * );
    llvm::Value * process( llvm::Instruction * );

    Type _type;
    //SubstitutionBuilder builder;
};

PassMeta substitution_pass() {
    return Substitution::meta();
}

} // namespace abstract
} // namespace lart

