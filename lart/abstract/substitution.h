// -*- C++ -*- (c) 2016 Henrich Lauko <xlauko@mail.muni.cz>
#pragma once

#include <lart/abstract/domains/domains.h>
#include <lart/abstract/domains/zero.h>
#include <lart/abstract/domains/sym.h>

#include <lart/abstract/substituter.h>
#include <lart/support/pass.h>
#include <lart/support/meta.h>

DIVINE_RELAX_WARNINGS
#include <llvm/Pass.h>
DIVINE_UNRELAX_WARNINGS

namespace lart {
namespace abstract {

struct Substitution
{
    Substitution() {}

    virtual ~Substitution() {}

    static PassMeta meta() {
	    return passMeta< Substitution >(
            "Substitution",
            "Substitutes abstract values by concrete abstraction." );
    }

    void run( llvm::Module & );

private:
    void init( llvm::Module & m ) {
        if ( Zero::isPresent( m ) )
            subst.registerAbstraction( std::make_shared< Zero >( m ) );
        if ( Symbolic::isPresent( m ) )
            subst.registerAbstraction( std::make_shared< Symbolic >( m ) );
    }


    void process( llvm::Value * );

    void changeReturn( llvm::Function * );

    Substituter subst;
};

static inline PassMeta substitution_pass() {
    return Substitution::meta();
}

} // namespace abstract
} // namespace lart

