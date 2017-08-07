// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/domains/domains.h>
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
    Substitution( Domain::Value domain ) : domain( domain ) {}

    Substitution( Substitution && s ) = default;

    virtual ~Substitution() {}

    static PassMeta meta() {
    	return passMetaC( "Substitution",
                "Substitutes abstract values by concrete abstraction.",
                []( llvm::ModulePassManager &mgr, std::string opt ) {
                    auto dom = Domain::value( opt );
                    if ( dom.isNothing() )
                        throw std::runtime_error( "unknown alias-analysis type: " + opt );
                    return mgr.addPass( Substitution( dom.value() ) );
                } );
    }

    llvm::PreservedAnalyses run( llvm::Module & ) override;

private:
    void init( llvm::Module & );
    void process( llvm::Value * );

    void changeReturn( llvm::Function * );

    Domain::Value domain;
    SubstitutionBuilder builder;
};

static inline PassMeta substitution_pass() {
    return Substitution::meta();
}

} // namespace abstract
} // namespace lart

