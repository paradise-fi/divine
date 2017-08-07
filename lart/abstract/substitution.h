// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/domains/trivial.h>
#include <lart/abstract/domains/zero.h>
#include <lart/abstract/domains/sym.h>

#include <lart/abstract/sbuilder.h>
#include <lart/support/pass.h>
#include <lart/support/meta.h>

DIVINE_RELAX_WARNINGS
#include <llvm/Pass.h>
DIVINE_UNRELAX_WARNINGS

namespace lart {
namespace abstract {

struct Substitution : lart::Pass
{
    enum Type { Trivial, Zero, Symbolic };

    Substitution( Type t ) : type( t ) {}

    Substitution( Substitution && s )
        : type( s.type ), builder( std::move( s.builder ) ) {}

    virtual ~Substitution() {}

    static PassMeta meta() {
    	return passMetaC( "Substitution",
                "Substitutes abstract values by concrete abstraction.",
                []( llvm::ModulePassManager &mgr, std::string opt ) {
                    return mgr.addPass( Substitution( getType( opt ) ) );
                } );
    }

    static Type getType( std::string opt ) {
        if ( opt == "trivial" )
            return Type::Trivial;
        if ( opt == "zero" )
            return Type::Zero;
        if ( opt == "sym" )
            return Type::Symbolic;
        std::cerr << "Unsupported abstraction " << opt <<std::endl;
        std::exit( EXIT_FAILURE );
    }

    llvm::PreservedAnalyses run( llvm::Module & ) override;

private:
    void init( llvm::Module & );
    void process( llvm::Value * );

    void changeReturn( llvm::Function * );

    Type type;
    SubstitutionBuilder builder;
};

static inline PassMeta substitution_pass() {
    return Substitution::meta();
}

} // namespace abstract
} // namespace lart

